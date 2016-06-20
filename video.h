#ifndef _VIDEO_H_
#define _VIDEO_H_
#include "bit_reader.h"
class VideoDecoder {
private:
    /* huffman trees */
    HuffmanTree ht_macroblock_addr;
    HuffmanTree ht_coded_block_pattern;
    HuffmanTree ht_motion_vector;
    HuffmanTree ht_dct_dc_size_luminance;
    HuffmanTree ht_dct_dc_size_chrominance;
    /* sequence header */
    int h_size, v_size;
    byte per_ratio, picture_rate;
    int bit_rate;
    int vbv_buffer_size;
    bool const_param_flag;
    byte intra_quant_matrix[64];
    byte non_intra_quant_matrix[64];

    /* slice */
    byte slice_vert_pos;
    byte quant_scale;
public:
    VideoDecoder();
    void video_sequence(BitReader &stream);
    void sequence_header(BitReader &stream);
    void group_of_pictures(BitReader &stream);
    void picture(BitReader &stream);
    void slice(BitReader &stream);
    void macroblock(BitReader &stream);
    void block(BitReader &stream);
};
#endif
