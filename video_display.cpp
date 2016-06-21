#include "CImg.h"
#include "video.h"
using namespace cimg_library;

void VideoDecoder::display(YCbCrBuffer *buf) {
    byte buffer[3][320][256];
    for(int i=0; i<320; ++i)
        for(int j=0; j<256; ++j) {
            buffer[0][i][j] = std::max(0., std::min(255., buf->y[i][j] + 1.402*buf->cr[i/2][j/2]));
            buffer[1][i][j] = std::max(0., std::min(255., buf->y[i][j] - 0.34414*buf->cb[i/2][j/2] - 0.71414*buf->cr[i/2][j/2]));
            buffer[2][i][j] = std::max(0., std::min(255., buf->y[i][j] + 1.772*buf->cb[i/2][j/2]));
        }
    main_disp.display(CImg<byte>(&buffer[0][0][0], 320, 256, 1, 3, true));
}
