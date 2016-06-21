#ifndef _VIDEO_H_
#define _VIDEO_H_
#include <tuple>
#include <vector>
#include "bit_reader.h"
extern const byte mask_macroblock_quant;
extern const byte mask_macroblock_motion_f;
extern const byte mask_macroblock_motion_b;
extern const byte mask_macroblock_pattern;
extern const byte mask_macroblock_intra;
class VideoDecoder {
private:
    /* huffman trees */
    HuffmanTree ht_macroblock_addr;
    HuffmanTree ht_coded_block_pattern;
    HuffmanTree ht_motion_vector;
    HuffmanTree ht_dct_dc_size_luminance;
    HuffmanTree ht_dct_dc_size_chrominance;
    HuffmanTree ht_intra_macroblock_type;
    HuffmanTree ht_p_macroblock_type;
    HuffmanTree ht_b_macroblock_type;
    HuffmanTree ht_run_level_ind;
    std::vector<int> run_list, level_list;

    /* sequence header */
    int h_size, v_size;
    byte per_ratio, picture_rate;
    int bit_rate;
    int vbv_buffer_size;
    bool const_param_flag;
    byte *intra_quant_matrix;
    byte *non_intra_quant_matrix;

    /* picture */
    int tmp_ref;
    byte coding_type;
    int vbv_delay;
    bool full_pel_forward_vector;
    byte forward_r_size;
    byte forward_f;
    bool full_pel_backward_vector;
    byte backward_r_size;
    byte backward_f;

    /* slice */
    byte slice_vert_pos;
    byte quant_scale;

    /* macroblock */
    byte macroblock_type;
    int motion_h_f_code, motion_h_f_r;
    int motion_v_f_code, motion_v_f_r;
    int motion_h_b_code, motion_h_b_r;
    int motion_v_b_code, motion_v_b_r;

    /* block */
    int dct_zz[64];

    /* prev */

    std::tuple<int, int> decode_run_level(BitReader &stream, bool first=false);
    
public:
    VideoDecoder();
    void video_sequence(BitReader &stream);
    void sequence_header(BitReader &stream);
    void group_of_pictures(BitReader &stream);
    void picture(BitReader &stream);
    void slice(BitReader &stream);
    void macroblock(BitReader &stream);
    void block(int index, BitReader &stream);
};
#endif