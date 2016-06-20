#include "video.h"

void read_table(HuffmanTree &ht, const char *filename) {
    FILE *file;
    file = fopen(filename, "r");
    char code[32];
    int val;
    while(fscanf(file, "%s %d", code, &val) != EOF) {
        ht.insertNode(code, val); // insert to huffman tree
    }
    return;
}

VideoDecoder::VideoDecoder() {
    /* load huffman tables */
    read_table(ht_macroblock_addr, "huffman_tables/macroblock_addr.txt");
    read_table(ht_coded_block_pattern, "huffman_tables/coded_block_pattern.txt");
    read_table(ht_motion_vector, "huffman_tables/motion_vector.txt");
    read_table(ht_dct_dc_size_luminance, "huffman_tables/dct_dc_size_luminance.txt");
    read_table(ht_dct_dc_size_chrominance, "huffman_tables/dct_dc_size_chrominance.txt");
}
