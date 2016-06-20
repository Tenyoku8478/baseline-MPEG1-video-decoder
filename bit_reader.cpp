#include <cassert>
#include <cstring>
#include "bit_reader.h"

BitReader::BitReader(FILE *__file) {
    file = __file;
    now = prev = 0;
    mask = prev_mask = 0;
}

byte BitReader::read() {
    byte res;
    if(mask == 0) {
        fread(&now, 1, 1, file); mask = 0x80;
    }
    res = (now&mask)!=0;
    mask >>= 1;
    return res;
}

int BitReader::read(int nbits) {
    int res = 0;
    for(int i=0; i<nbits; ++i)
        res = (res<<1)+read();
    return res;
}

void BitReader::read(byte *buf, size_t size) {
    for(size_t i=0; i<size; ++i)
        buf[i] = read(8);
}

bool BitReader::next_bits(const char *const bits, bool keep) {
    if(keep) save();

    bool res = true;
    for(int i=0; bits[i] and res; ++i) {
        byte bit = read();
        assert(bits[i]=='0' || bits[i]=='1');
        if(bit != (bits[i]-'0'))
            res = false;
    }

    if(keep) restore();
    return res;
}

void BitReader::next_start_code() {
    mask = 0; // drop incomplele byte
    while(!next_bits(start_code))
        fseek(file, 1, SEEK_CUR);
}

void BitReader::save() {
    prev = now;
    prev_mask = mask;
    fgetpos(file, &prev_pos);
}

void BitReader::restore() {
    now = prev;
    mask = prev_mask;
    fsetpos(file, &prev_pos);
}

void HuffmanTree::insertNode(const char *const path, int val) {
    Node *ptr = root;
    for(int i=0; path[i]; ++i) {
        int bit = path[i]-'0';
        if(ptr->child[bit] == nullptr) {
            Node *newNode = new Node();
            ptr->child[bit] = newNode;
        }
        ptr = ptr->child[bit];
    }
    ptr->val = val;
    ptr->leaf = true;
    return;
}

HuffmanTree::HuffmanTree() {
    root = new Node();
}

byte HuffmanTree::decode(BitReader &bitReader) {
    Node *now = root;
    while(!now->leaf) {
        now = now->child[bitReader.read()];
    }
    return now->val;
}
