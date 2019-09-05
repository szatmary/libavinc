#include "../libavinc.hpp"

int main(int argc, char** argv)
{
    std::string inurl = 1 < argc ? argv[1] : std::string();
    std::string mp4url = 2 < argc ? argv[2] : std::string();
    auto media = libav::avformat_open_input(inurl, { { "test_key", "test_value" } });
    if (!media) {
        std::cerr << "Failed to open '" << inurl << "'" << std::endl;
        return EXIT_FAILURE;
    }

    ::av_dump_format(media.get(), -1, inurl.c_str(), 0);
    libav::av_open_best_streams(media);
    if (media.open_streams.empty()) {
        std::cerr << "Media does not containe any usable streams" << std::endl;
        return EXIT_FAILURE;
    }

    auto mp4 = libav::avformat_open_output(mp4url, "mp4");
    if (!mp4) {
        std::cerr << "Failed to create '" << mp4url << "'" << std::endl;
        return EXIT_FAILURE;
    }

    std::map<int, int> stream_map;
    for (const auto& stream : media.open_streams) {
        auto codecpar = media->streams[stream.first]->codecpar;
        stream_map.emplace(stream.first, avformat_new_stream(mp4, codecpar));
    }

    if (::avformat_write_header(mp4.get(), nullptr) < 0) {
        std::cerr << "Failed to write file header" << std::endl;
        return EXIT_FAILURE;
    }

    for (const auto& pkt : media) {
        av_interleaved_write_frame(mp4, stream_map[pkt.stream_index], pkt);
    }
    ::av_write_trailer(mp4.get());

    return EXIT_SUCCESS;
}
