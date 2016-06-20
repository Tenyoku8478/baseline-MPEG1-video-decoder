#include <cstdio>
#include <cassert>
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
    EAT(sequence_end_code);
}

void VideoDecoder::sequence_header(BitReader &stream) {
    LOG("sequence header");
    EAT(sequence_header_code);
    h_size = stream.read(12);
    v_size = stream.read(12);
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
    int tmp_ref = stream.read(10);
    byte coding_type = stream.read(3);
    int vbv_delay = stream.read(16);
    if(coding_type == 2 || coding_type == 3) {
        byte full_pel_forward_vector = stream.read();
        byte forward_f_code = stream.read(3);
    }
    if(coding_type == 3) {
        byte full_pel_backward_vector = stream.read();
        byte backward_f_code = stream.read(3);
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
    do {
        macroblock(stream);
    } while(!stream.next_bits("000000000000000000000000"));
    stream.next_start_code();
}

void VideoDecoder::macroblock(BitReader &stream) {
    while(stream.next_bits(macroblock_stuffing))
        EAT(macroblock_stuffing);
    int macroblock_addr_increment = 0;
    while(stream.next_bits(macroblock_escape)) {
        EAT(macroblock_escape);
        macroblock_addr_increment += 33;
    }
    macroblock_addr_increment += ht_macroblock_addr.decode(stream);
    printf("%d\n", macroblock_addr_increment);
    assert(false);
    return;
}
