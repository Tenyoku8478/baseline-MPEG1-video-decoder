#include "CImg.h"
#include "video.h"
using namespace cimg_library;

void VideoDecoder::display(YCbCrBuffer *buf) {
    byte buffer[3][256][320];
    for(int i=0; i<320; ++i)
        for(int j=0; j<256; ++j) {
            buffer[0][j][i] = std::max(0., std::min(255., 255./219*(buf->y[j][i]-16) + 255./112*0.701*(buf->cr[j/2][i/2]-128)));
            buffer[1][j][i] = std::max(0., std::min(255., 255./219*(buf->y[j][i]-16) - 255./112*0.886*0.114/0.587*(buf->cb[j/2][i/2]-128) - 255./112*0.701*0.299/0.587*(buf->cr[j/2][i/2]-128)));
            buffer[2][j][i] = std::max(0., std::min(255., 255./219*(buf->y[j][i]-16) + 255./112*0.886*(buf->cb[j/2][i/2]-128)));
            /*
            printf("%f %f %f\n", buf->y[i][j], buf->cb[i/2][j/2], buf->cr[i/2][j/2]);
            printf("%d %d %d\n", buffer[0][i][j], buffer[1][i][j], buffer[2][i][j]);
            */
        }
    main_disp.display(CImg<byte>(&buffer[0][0][0], 320, 256, 1, 3, true));
}
