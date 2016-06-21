#include <cassert>
#include "video.h"

#define EAT(X) assert(stream.next_bits(X, false));

void read_table(HuffmanTree &ht, const char *filename) {
    FILE *file;
    file = fopen(filename, "r");
    assert(file != nullptr);
    char code[32];
    int val;
    while(fscanf(file, "%s %d", code, &val) != EOF) {
        ht.insertNode(code, val); // insert to huffman tree
    }
    fclose(file);
    return;
}

const byte mask_macroblock_quant = 0x10;
const byte mask_macroblock_motion_f = 0x08;
const byte mask_macroblock_motion_b = 0x04;
const byte mask_macroblock_pattern = 0x02;
const byte mask_macroblock_intra = 0x01;
void read_macroblock_type_table(HuffmanTree &ht, const char *filename) {
    FILE *file;
    file = fopen(filename, "r");
    assert(file != nullptr);
    char code[32];
    int tmp;
    while(fscanf(file, "%s", code) != EOF) {
        int val = 0;
        for(int i=0; i<5; ++i) {
            fscanf(file, "%d", &tmp);
            val = (val<<1) + tmp;
        }
        ht.insertNode(code, val); // insert to huffman tree
    }
    fclose(file);
    return;
}

void read_run_level_table(HuffmanTree &ht, std::vector<int> &run_list, std::vector<int> &level_list, const char *filename) {
    run_list.resize(128);
    level_list.resize(128);
    FILE *file;
    file = fopen(filename, "r");
    assert(file != nullptr);
    char code[32];
    int i=0, run, level;
    while(fscanf(file, "%s %d %d", code, &run, &level) != EOF) {
        run_list[i] = run;
        level_list[i] = level;
        ht.insertNode(code, i++); // insert to huffman tree
    }
    fclose(file);
    return;
}

std::tuple<int, int> VideoDecoder::decode_run_level(BitReader &stream, bool first) {
    int run, level;
    if(stream.next_bits("000001")) { // escape - fixed length
        EAT("000001");
        run = stream.read(6);
        int tmp = stream.read(8);
        if(tmp == 0x00) { // negative
            level = stream.read(8);
        }
        else if(tmp == 0x80) { // >= 128
            level = stream.read(8)-256;
        }
        else { // otherwise
            level = tmp;
        }
    }
    else {
        int ind = ht_run_level_ind.decode(stream);
        run = run_list[ind];
        level = level_list[ind];
        if(!first && run == 0 && level == 1) {
            EAT("1") // spec NOTE2 and NOTE3
        }
        bool s = stream.read();
        if(s) level = - level;
    }
    return std::make_tuple(run, level);
}

VideoDecoder::VideoDecoder() {
    intra_quant_matrix = (byte*)malloc(64);
    non_intra_quant_matrix = (byte*)malloc(64);
    /* load huffman tables */
    read_table(ht_macroblock_addr, "huffman_tables/macroblock_addr.txt");
    read_table(ht_coded_block_pattern, "huffman_tables/coded_block_pattern.txt");
    read_table(ht_motion_vector, "huffman_tables/motion_vector.txt");
    read_table(ht_dct_dc_size_luminance, "huffman_tables/dct_dc_size_luminance.txt");
    read_table(ht_dct_dc_size_chrominance, "huffman_tables/dct_dc_size_chrominance.txt");
    read_macroblock_type_table(ht_intra_macroblock_type, "huffman_tables/intra_macroblock_type.txt");
    read_macroblock_type_table(ht_p_macroblock_type, "huffman_tables/p_macroblock_type.txt");
    read_macroblock_type_table(ht_b_macroblock_type, "huffman_tables/b_macroblock_type.txt");
    read_run_level_table(ht_run_level_ind, run_list, level_list, "huffman_tables/run_level.txt");
}
