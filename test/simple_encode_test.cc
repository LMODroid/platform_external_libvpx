#include <math.h>
#include <memory>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "vp9/simple_encode.h"

namespace {
const int w = 352;
const int h = 288;
const int frame_rate_num = 30;
const int frame_rate_den = 1;
const int target_bitrate = 1000;
const int num_frames = 17;
// TODO(angiebird): Figure out how to upload test video to our codebase
const char infile_path[] = "bus_352x288_420_f20_b8.yuv";

static double get_bit_rate_in_kpbs(size_t bit_size, int num_frames,
                                   int frame_rate_num, int frame_rate_den) {
  return static_cast<double>(bit_size) / num_frames * frame_rate_num /
         frame_rate_den / 1000.;
}

TEST(SimpleEncode, ComputeFirstPassStats) {
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, infile_path);
  simple_encode.ComputeFirstPassStats();
  std::vector<std::vector<double>> frame_stats =
      simple_encode.ObserveFirstPassStats();
  EXPECT_EQ(frame_stats.size(), static_cast<size_t>(num_frames));
  size_t data_num = frame_stats[0].size();
  // Read ObserveFirstPassStats before changing FIRSTPASS_STATS.
  EXPECT_EQ(data_num, static_cast<size_t>(25));
  for (size_t i = 0; i < frame_stats.size(); ++i) {
    EXPECT_EQ(frame_stats[i].size(), data_num);
    // FIRSTPASS_STATS's first element is frame
    EXPECT_EQ(frame_stats[i][0], i);
    // FIRSTPASS_STATS's last element is count, and the count is 1 for single
    // frame stats
    EXPECT_EQ(frame_stats[i][data_num - 1], 1);
  }
}

TEST(SimpleEncode, GetCodingFrameNum) {
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, infile_path);
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_EQ(num_coding_frames, 19);
}

TEST(SimpleEncode, EncodeFrame) {
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, infile_path);
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_GE(num_coding_frames, num_frames);
  // The coding frames include actual show frames and alternate reference
  // frames, i.e. no show frame.
  int ref_num_alternate_refereces = num_coding_frames - num_frames;
  int num_alternate_refereces = 0;
  simple_encode.StartEncode();
  size_t total_data_bit_size = 0;
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameResult encode_frame_result;
    if (i == 0) {
      EXPECT_EQ(encode_frame_result.show_idx, 0);
      EXPECT_EQ(encode_frame_result.frame_type, kKeyFrame)
          << "The first coding frame should be key frame";
    }
    simple_encode.EncodeFrame(&encode_frame_result);
    if (encode_frame_result.frame_type == kAlternateReference) {
      ++num_alternate_refereces;
    }
    EXPECT_GE(encode_frame_result.show_idx, 0);
    EXPECT_LT(encode_frame_result.show_idx, num_frames);
    if (i == num_coding_frames - 1) {
      EXPECT_EQ(encode_frame_result.show_idx, num_frames - 1)
          << "The last coding frame should be the last display order";
    }
    EXPECT_GE(encode_frame_result.psnr, 34)
        << "The psnr is supposed to be greater than 34 given the "
           "target_bitrate 1000 kbps";
    total_data_bit_size += encode_frame_result.coding_data_bit_size;
  }
  EXPECT_EQ(num_alternate_refereces, ref_num_alternate_refereces);
  double bitrate = get_bit_rate_in_kpbs(total_data_bit_size, num_frames,
                                        frame_rate_num, frame_rate_den);
  EXPECT_LE(fabs(target_bitrate - bitrate), 150);
  simple_encode.EndEncode();
}

TEST(SimpleEncode, EncodeFrameWithQuantizeIndex) {
  SimpleEncode simple_encode(w, h, frame_rate_num, frame_rate_den,
                             target_bitrate, num_frames, infile_path);
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    int assigned_quantize_index = 100 + i;
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrameWithQuantizeIndex(&encode_frame_result,
                                               assigned_quantize_index);
    EXPECT_EQ(encode_frame_result.quantize_index, assigned_quantize_index);
  }
  simple_encode.EndEncode();
}

}  // namespace
