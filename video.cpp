#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>
#include <algorithm>
#include "magic_code.h"
#include "bit_reader.h"
#include "video.h"

#define EAT(X) assert(stream.next_bits(X, false));
#define LOG(MSG) puts(MSG);

const char start_code[]           = "000000000000000000000001";
const char picture_start_code[]   = "00000000000000000000000100000000";
const char user_data_start_code[] = "00000000000000000000000110110010";
const char sequence_header_code[] = "00000000000000000000000110110011";
const char sequence_error_code[]  = "00000000000000000000000110110100";
const char extension_start_code[] = "00000000000000000000000110110101";
const char sequence_end_code[]    = "00000000000000000000000110110111";
const char group_start_code[]     = "00000000000000000000000110111000";
const char macroblock_stuffing[]  = "00000001111";
const char macroblock_escape[]    = "00000001000";

/* parse sequance layer */
void VideoDecoder::video_sequence(BitReader &stream) {
    LOG("video sequence");
    stream.next_start_code();
    do {
        sequence_header(stream);
        do {
            group_of_pictures(stream);
        } while(stream.next_bits(group_start_code));
    } while(stream.next_bits(sequence_header_code));
    display(b_buf); // last backward frame
    EAT(sequence_end_code);
}

/* parse sequance header */
void VideoDecoder::sequence_header(BitReader &stream) {
    LOG("sequence header");
    EAT(sequence_header_code);
    h_size = stream.read(12);
    v_size = stream.read(12);
    mb_width = (h_size+15)/16; // /16 & ceil
    per_ratio = stream.read(4);
    picture_rate = stream.read(4);
    bit_rate = stream.read(18);
    EAT("1"); //marker bit
    vbv_buffer_size = stream.read(10);
    const_param_flag = stream.read();
    bool load_intra_quantizer_matrix = stream.read();
    // check flag bit
    if(load_intra_quantizer_matrix) {
        stream.read(intra_quant_matrix, 64);
    }
    bool load_non_intra_quantizer_matrix = stream.read();
    // check flag bit
    if(load_non_intra_quantizer_matrix) {
        stream.read(non_intra_quant_matrix, 64);
    }
    // align byte
    stream.next_start_code();
    if(stream.next_bits(extension_start_code)) {
        EAT(extension_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // sequence_extension_data
        }
        stream.next_start_code();
    }
    if(stream.next_bits(user_data_start_code)) {
        EAT(user_data_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // user_data
        }
        stream.next_start_code();
    }
}

/* parse gop layer */
void VideoDecoder::group_of_pictures(BitReader &stream) {
    LOG("group of pictures");
    EAT(group_start_code);
    int time_code = stream.read(25);
    bool closed_gop = stream.read();
    bool broken_link = stream.read();
    assert(!broken_link);
    stream.next_start_code();
    if(stream.next_bits(extension_start_code)) {
        EAT(extension_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // sequence_extension_data
        }
        stream.next_start_code();
    }
    if(stream.next_bits(user_data_start_code)) {
        EAT(user_data_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // user_data
        }
        stream.next_start_code();
    }
    do {
        picture(stream);
    } while(stream.next_bits(picture_start_code));
}

/* check if the following bits are slice start code */
inline bool is_slice_start_code(BitReader &stream) {
    stream.save();
    if(!stream.next_bits(start_code, false)) {
        stream.restore();
        return false;
    }
    byte code = stream.read(8);
    stream.restore();
    return 0x01 <= code and code <= 0xAF;
}

/* parse picture layer */
void VideoDecoder::picture(BitReader &stream) {
    LOG("picture");
    EAT(picture_start_code);
    tmp_ref = stream.read(10);
    coding_type = stream.read(3);

    /* 3-Frame Buffers Algorithm - before decode*/
    if(coding_type <= 2) {
        std::swap(f_buf, b_buf);
        display(f_buf);
    }

    vbv_delay = stream.read(16);
    if(coding_type == 2 || coding_type == 3) { // P, B Frame
        // read forward size
        full_pel_forward_vector = stream.read();
        byte forward_f_code = stream.read(3);
        assert(forward_f_code != 0);
        forward_r_size = forward_f_code - 1;
        forward_f = 1 << forward_r_size;
    }
    if(coding_type == 3) { // B frame
        // read backward size
        full_pel_backward_vector = stream.read();
        byte backward_f_code = stream.read(3);
        assert(backward_f_code != 0);
        backward_r_size = backward_f_code - 1;
        backward_f = 1 << backward_r_size;
    }
    while(stream.next_bits("1")) {
        EAT("1"); // extra_bit_picture
        stream.read(8); // extra_info_picture
    }
    EAT("0"); // extra_bit_picture

    stream.next_start_code();
    if(stream.next_bits(extension_start_code)) {
        EAT(extension_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // sequence_extension_data
        }
        stream.next_start_code();
    }
    if(stream.next_bits(user_data_start_code)) {
        EAT(user_data_start_code);
        while (!stream.next_bits(start_code)) {
            stream.read(8); // user_data
        }
        stream.next_start_code();
    }
    do {
        slice(stream);
    } while(is_slice_start_code(stream));

    /* 3-Frame Buffers Algorithm - after decode*/
    if(coding_type <= 2) {
        std::swap(c_buf, b_buf);
    }
    else {
        display(c_buf);
    }
}

/* parse slice layer */
void VideoDecoder::slice(BitReader &stream) {
    LOG("slice");
    EAT(start_code);
    slice_vert_pos = stream.read(8);
    quant_scale = stream.read(5);
    while(stream.next_bits("1")) {
        EAT("1"); // extra_bit_slice
        stream.read(8); // extra_info_slice 
    }
    EAT("0"); // extra_bit_slice

    /* reset previous variables */
    past_intra_addr = -2;
    macroblock_addr = (slice_vert_pos-1)*mb_width-1;

    do {
        macroblock(stream);
    } while(!stream.next_bits("00000000000000000000000"));
    stream.next_start_code();
}

/* parse macroblock layer */
void VideoDecoder::macroblock(BitReader &stream) {
    //LOG("macroblock");
    while(stream.next_bits(macroblock_stuffing))
        EAT(macroblock_stuffing);
    int macroblock_addr_increment = 0;
    while(stream.next_bits(macroblock_escape)) {
        // add 33 when meet escape code
        EAT(macroblock_escape);
        macroblock_addr_increment += 33;
    }
    macroblock_addr_increment += ht_macroblock_addr.decode(stream);
    macroblock_addr += macroblock_addr_increment;
    // get coded macroblock type
    if(coding_type == 1)
        macroblock_type = ht_intra_macroblock_type.decode(stream);
    else if(coding_type == 2)
        macroblock_type = ht_p_macroblock_type.decode(stream);
    else if(coding_type == 3)
        macroblock_type = ht_b_macroblock_type.decode(stream);
    else
        assert(false);
    // check quant_scale field flag
    if(macroblock_type & mask_macroblock_quant)
        quant_scale = stream.read(5);
    // check motion forward field flag
    if(macroblock_type & mask_macroblock_motion_f) {
        motion_h_f_code = ht_motion_vector.decode(stream);
        if((forward_f != 1) and
            (motion_h_f_code != 0))
            motion_h_f_r = stream.read(forward_r_size);
        motion_v_f_code = ht_motion_vector.decode(stream);
        if((forward_f != 1) and
            (motion_v_f_code != 0))
            motion_v_f_r = stream.read(forward_r_size);
    }
    // check motion backward field flag
    if(macroblock_type & mask_macroblock_motion_b) {
        motion_h_b_code = ht_motion_vector.decode(stream);
        if((backward_f != 1) and
            (motion_h_b_code != 0))
            motion_h_b_r = stream.read(backward_r_size);
        motion_v_b_code = ht_motion_vector.decode(stream);
        if((backward_f != 1) and
            (motion_v_b_code != 0))
            motion_v_b_r = stream.read(backward_r_size);
    }
    // init cbp with all '1'
    byte cbp = (1<<6) - 1;
    if(macroblock_type & mask_macroblock_pattern) {
        cbp = ht_coded_block_pattern.decode(stream);
    }
    for(int i=0; i<6; ++i) {
        if(cbp & (1<<(5-i)))
            block(i, stream);
        else {
            memset(dct_zz, 0, sizeof(dct_zz));
        }
        decode_block(i);
        write_block(i);
    }

    // update past_intra_addr
    if(macroblock_type & mask_macroblock_intra)
        past_intra_addr = macroblock_addr;

    if(coding_type == 4)
        EAT("1");
    return;
}

void VideoDecoder::block(int index, BitReader &stream) {
    //LOG("block");
    memset(dct_zz, 0, sizeof(dct_zz));
    int i=0;
    if(macroblock_type & mask_macroblock_intra) {
        // intra block
        int size = 0;
        if(index<4) {
            size = ht_dct_dc_size_luminance.decode(stream);
        }
        else {
            size = ht_dct_dc_size_chrominance.decode(stream);
        }
        if(size == 0) {
            dct_zz[0] = 0;
        }
        else {
            int dct_dc_diff = stream.read(size);
            if(dct_dc_diff & (1<<(size-1))) dct_zz[0] = dct_dc_diff;
            else dct_zz[0] = (-1 << size) | (dct_dc_diff+1);
        }
    }
    else {
        // non-intra block
        int run, level;
        std::tie(run, level) = decode_run_level(stream, true);
        i = run;
        dct_zz[i] = level;
    }
    if(coding_type != 4) {
        // ac coefs
        while(!stream.next_bits("10")) {
            int run, level;
            std::tie(run, level) = decode_run_level(stream);
            i = i+run+1;
            assert(i < 64);
            dct_zz[i] = level;
        }
        EAT("10");
    }
};

inline void idct(double res[8], double mat[8]) {
    double c2 = 2*cos(M_PI/8), c4 = 2*cos(2*M_PI/8), c6 = 2*cos(3*M_PI/8);
    double sq8 = sqrt(8);

    // B1
    double a0 = (1./8*mat[0])*sq8;
    double a1 = (1./8*mat[4])*sq8;
    double a2 = (1./8*mat[2] - 1./8*mat[6])*sq8;
    double a3 = (1./8*mat[2] + 1./8*mat[6])*sq8;
    double a4 = (1./8*mat[5] - 1./8*mat[3])*sq8;
    double temp1 = (1./8*mat[1] + 1./8*mat[7])*sq8;
    double temp2 = (1./8*mat[3] + 1./8*mat[5])*sq8;
    double a5 = temp1 - temp2;
    double a6 = (1./8*mat[1] - 1./8*mat[7])*sq8;
    double a7 = temp1+temp2;

    // M
    double b0 = a0;
    double b1 = a1;
    double b2 = a2*c4;
    double b3 = a3;
    double Q = c2-c6, R = c2+c6, temp4 = c6*(a4+a6);
    double b4 = -Q*a4 - temp4;
    double b5 = a5*c4;
    double b6 = R*a6 - temp4;
    double b7 = a7;

    // A1
    double temp3 = b6 - b7;
    double n0 = temp3 - b5;
    double n1 = b0 - b1;
    double n2 = b2 - b3;
    double n3 = b0 + b1;
    double n4 = temp3;
    double n5 = b4;
    double n6 = b3;
    double n7 = b7;

    // A2
    double m0 = n7;
    double m1 = n0;
    double m2 = n4;
    double m3 = n1 + n2;
    double m4 = n3 + n6;
    double m5 = n1 - n2;
    double m6 = n3 - n6;
    double m7 = n5 - n0;

    // A3
    res[0] = m4 + m0;
    res[1] = m3 + m2;
    res[2] = m5 - m1;
    res[3] = m6 - m7;
    res[4] = m6 + m7;
    res[5] = m5 + m1;
    res[6] = m3 - m2;
    res[7] = m4 - m0;
    return;
}

inline void transpose(double mat[8][8]) {
    for(int i=0; i<7; ++i)
        for(int j=i+1; j<8; ++j)
            std::swap(mat[i][j], mat[j][i]);
    return;
}

inline void idct2d(double mat[8][8]) {
    double row[8][8];
    memcpy(row, mat, sizeof(row));
    for(int i=0; i<8; ++i)
        idct(row[i], mat[i]);
    transpose(row);
    for(int i=0; i<8; ++i)
        idct(mat[i], row[i]);
    transpose(mat);
    return;
}

void VideoDecoder::decode_block(int index) {
    #define SIGN(x) ((x > 0) - (x < 0))
    // recontruct dct ac component
    for(int m=0; m<8; ++m) {
        for(int n=0; n<8; ++n) {
            int i = scan[m][n];
            int tmp;
            tmp = (2*dct_zz[i]*quant_scale*intra_quant_matrix[i])/16;
            if((tmp & 1) == 0)
                tmp = tmp - SIGN(tmp);
            if(tmp > 2047) tmp = 2047;
            if(tmp < -2048) tmp = -2048;
            block_buf[m][n] = tmp;
        }
    }

    // reconstruct dct dc component
    double *dct_dc_past;
    if(index < 4) dct_dc_past = &dct_dc_y_past;
    else if(index == 4) dct_dc_past = &dct_dc_cb_past;
    else dct_dc_past = &dct_dc_cr_past;

    block_buf[0][0] = dct_zz[0]*8;
    if((index == 0 || index > 3) && macroblock_addr-past_intra_addr > 1) {
        // first block
        block_buf[0][0] = 128*8 + block_buf[0][0];
    }
    else {
        // not first block
        block_buf[0][0] = *dct_dc_past + block_buf[0][0];
    }
    *dct_dc_past = block_buf[0][0];
    idct2d(block_buf);
}

void VideoDecoder::write_block(int index) {
    // calculate row, column
    int mb_row = macroblock_addr/mb_width;
    int mb_col = macroblock_addr%mb_width;
    if(index <=3) {
        // Y
        int top = mb_row*16;
        int left = mb_col*16;
        switch(index) {
            case 0: break;
            case 1: left+=8; break;
            case 2: top+=8; break;
            case 3: left+=8, top+=8; break;
            default: assert(false);
        }
        for(int i=0; i<8; ++i)
            for(int j=0; j<8; ++j)
                c_buf->y[i+top][j+left] = std::max(0., block_buf[i][j]);
    }
    else {
        int top = mb_row*8;
        int left = mb_col*8;
        if(index == 4) {
            // Cb
            for(int i=0; i<8; ++i)
                for(int j=0; j<8; ++j)
                    c_buf->cb[i+top][j+left] = std::max(0., block_buf[i][j]);
        } else {
            // Cr
            for(int i=0; i<8; ++i)
                for(int j=0; j<8; ++j)
                    c_buf->cr[i+top][j+left] = std::max(0., block_buf[i][j]);
        }
    }
}
