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

void VideoDecoder::sequence_header(BitReader &stream) {
    LOG("sequence header");
    EAT(sequence_header_code);
    h_size = stream.read(12);
    v_size = stream.read(12);
    mb_width = (h_size+15)/16;
    per_ratio = stream.read(4);
    picture_rate = stream.read(4);
    bit_rate = stream.read(18);
    EAT("1"); //marker bit
    vbv_buffer_size = stream.read(10);
    const_param_flag = stream.read();
    bool load_intra_quantizer_matrix = stream.read();
    if(load_intra_quantizer_matrix) {
        stream.read(intra_quant_matrix, 64);
    }
    bool load_non_intra_quantizer_matrix = stream.read();
    if(load_non_intra_quantizer_matrix) {
        stream.read(non_intra_quant_matrix, 64);
    }
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

void VideoDecoder::group_of_pictures(BitReader &stream) {
    LOG("group of pictures");
    EAT(group_start_code);
    int time_code = stream.read(25);
    bool closed_gop = stream.read();
    bool broken_link = stream.read();
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

void VideoDecoder::picture(BitReader &stream) {
    LOG("picture");
    EAT(picture_start_code);
    tmp_ref = stream.read(10);
    coding_type = stream.read(3);

    /* 3-Frame Buffers Algorithm */
    if(coding_type <= 2) {
        std::swap(f_buf, b_buf);
        display(f_buf);
    }

    vbv_delay = stream.read(16);
    if(coding_type == 2 || coding_type == 3) {
        full_pel_forward_vector = stream.read();
        byte forward_f_code = stream.read(3);
        forward_r_size = forward_f_code - 1;
        forward_f = 1 << forward_r_size;
    }
    if(coding_type == 3) {
        full_pel_backward_vector = stream.read();
        byte backward_f_code = stream.read(3);
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

    /* 3-Frame Buffers Algorithm */
    if(coding_type <= 2) {
        std::swap(c_buf, b_buf);
    }
    else {
        display(c_buf);
    }
}

void VideoDecoder::slice(BitReader &stream) {
    LOG("slice");
    EAT(start_code);
    slice_vert_pos = stream.read(8);
    quant_scale = stream.read(5);
    while(stream.next_bits("1")) {
        EAT("1"); // extra_bit_slice
        stream.read(8); // extra_info_slice 
    }
    EAT("0");

    /* reset previous variables */
    past_intra_addr = -2;
    macroblock_addr = (slice_vert_pos-1)*mb_width-1;

    do {
        macroblock(stream);
    } while(!stream.next_bits("00000000000000000000000"));
    stream.next_start_code();
}

void VideoDecoder::macroblock(BitReader &stream) {
    //LOG("macroblock");
    while(stream.next_bits(macroblock_stuffing))
        EAT(macroblock_stuffing);
    int macroblock_addr_increment = 0;
    while(stream.next_bits(macroblock_escape)) {
        EAT(macroblock_escape);
        macroblock_addr_increment += 33;
    }
    macroblock_addr_increment += ht_macroblock_addr.decode(stream);
    macroblock_addr += macroblock_addr_increment;
    if(coding_type == 1)
        macroblock_type = ht_intra_macroblock_type.decode(stream);
    else if(coding_type == 2)
        macroblock_type = ht_p_macroblock_type.decode(stream);
    else if(coding_type == 3)
        macroblock_type = ht_b_macroblock_type.decode(stream);
    else
        assert(false);
    if(macroblock_type & mask_macroblock_quant)
        quant_scale = stream.read(5);
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
        int size = 0;
        if(index<4) {
            size = ht_dct_dc_size_luminance.decode(stream);
        }
        else {
            size = ht_dct_dc_size_chrominance.decode(stream);
        }
        int dct_dc_diff = stream.read(size);
        if(dct_dc_diff & (1<<(size-1))) dct_zz[0] = dct_dc_diff;
        else dct_zz[0] = (-1 << size) | (dct_dc_diff+1);
    }
    else {
        int run, level;
        std::tie(run, level) = decode_run_level(stream, true);
        i = run;
        dct_zz[i] = level;
    }
    if(coding_type != 4) {
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

inline void idct(int res[8], int mat[8]) {
    for(int i=0; i<8; ++i) {
        double now = ((double)mat[0])/sqrt(2);
        for(int j=1; j<8; ++j)
            now += mat[j]*cos((2*i+1)*j*M_PI/16);
        res[i] = now/2;
    }
    return;
}

inline void transpose(int mat[8][8]) {
    for(int i=0; i<7; ++i)
        for(int j=i+1; j<8; ++j)
            std::swap(mat[i][j], mat[j][i]);
    return;
}

inline void idct2d(int mat[8][8]) {
    int row[8][8];
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
    for(int m=0; m<8; ++m) {
        for(int n=0; n<8; ++n) {
            int i = scan[m][n];
            block_buf[m][n] = (2*dct_zz[i]*quant_scale*intra_quant_matrix[i])/16;
            if((block_buf[m][n] & 1) == 0)
                block_buf[m][n] = block_buf[m][n] - SIGN(block_buf[m][n]);
            if(block_buf[m][n] > 2047) block_buf[m][n] = 2047;
            if(block_buf[m][n] < -2048) block_buf[m][n] = -2048;
        }
    }
    int *dct_dc_past;
    if(index < 4) dct_dc_past = &dct_dc_y_past;
    else if(index == 4) dct_dc_past = &dct_dc_cb_past;
    else dct_dc_past = &dct_dc_cr_past;

    block_buf[0][0] = dct_zz[0]*8;
    if((index == 0 || index > 3) && macroblock_addr-past_intra_addr > 1)
        block_buf[0][0] = 128*8 + block_buf[0][0];
    else {
        block_buf[0][0] = *dct_dc_past + block_buf[0][0];
    }
    *dct_dc_past = block_buf[0][0];
    idct2d(block_buf);
}

void VideoDecoder::write_block(int index) {
    int mb_row = macroblock_addr/mb_width;
    int mb_col = macroblock_addr%mb_width;
    if(index <=3) {
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
                c_buf->y[i+top][j+left] = block_buf[i][j];
    }
    else {
        int top = mb_row*8;
        int left = mb_col*8;
        if(index == 4) {
            for(int i=0; i<8; ++i)
                for(int j=0; j<8; ++j)
                    c_buf->cb[i+top][j+left] = block_buf[i][j];
        } else {
            for(int i=0; i<8; ++i)
                for(int j=0; j<8; ++j)
                    c_buf->cr[i+top][j+left] = block_buf[i][j];
        }
    }
}
