#include <stdio.h>
#include <stdint.h>

#define MAKE_FOURCC(a,b,c,d) \
( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )


// Wave Header Format

// format 子块
struct SubChunkFormat
{
	uint32_t chunk;           // 固定为fmt ，大端存储
	uint32_t subchunk_size;   // 子块的大小，不包含chunk+subchunk_size字段
	uint16_t audio_format;    // 编码格式(Audio Format)，1代表PCM无损格式
	uint16_t channels;        // 声道数(Channels)，1或2
	uint32_t sample_rate;     // 采样率(Sample Rate)
	uint32_t byte_rate;       // 传输速率(Byte Rate)，每秒数据字节数，SampleRate * Channels * BitsPerSample / 8
	uint16_t block_align;     // 每个采样所需的字节数BlockAlign，BitsPerSample*Channels / 8
	uint16_t bits_per_sample; // 单个采样位深(Bits Per Sample)，可选8、16或32
	// uint16_t ex_size;         // 扩展块的大小，附加块的大小，非PCM编码需要
};

// data 子块
struct SubChunkData
{
	uint32_t chunk;         // 固定为data，大端存储
	uint32_t subchunk_size; // 子块的大小，不包含chunk+subchunk_size字段
};

struct WaveHeader
{
	uint32_t fourcc;     // 固定为RIFF，大端存储，所以需要使用MAKE_FOURCC宏处理
	uint32_t chunk_size; // 文件长度，不包含fourcc和chunk_size，即总文件长度-8字节
	uint32_t form_type;  // 固定为WAVE，大端存储，类型码(Form Type)，WAV文件格式标记，即"WAVE"四个字母

	SubChunkFormat subchunk_format; // fmt  子块
	SubChunkData subchunk_data;     // data 子块
};


static void print_usage(char* argv[]) {
    printf("\nusage: %s in.pcm out.wav sample_rate sample_bits channels\n", argv[ 0 ] );
    printf("\nin.pcm       : pcm file to input" );
    printf("\nout.wav      : wave file to output" );
	printf("\nsample_rate  : e.g. 8000,12000,16000,24000,32000,44100,48000...");
	printf("\nsample_bits  : e.g. 16");
	printf("\nchannels     : e.g. 1,2...");
    printf( "\n" );
}

void write_wave_header(FILE* fp, int pcm_file_size, uint32_t sample_rate, uint16_t sample_bits, uint16_t channels)
{
	WaveHeader header;
	header.fourcc = MAKE_FOURCC('R', 'I', 'F', 'F');
	fwrite(&header.fourcc, sizeof(char), sizeof(header.fourcc), fp);
	header.chunk_size = 36 + pcm_file_size; // 36 = 44 (header size) - 8(sizeof chunk+ sizeof chunk_size)  
	fwrite(&header.chunk_size, sizeof(char), sizeof(header.chunk_size), fp);
	header.form_type = MAKE_FOURCC('W', 'A', 'V', 'E');
	fwrite(&header.form_type, sizeof(char), sizeof(header.form_type), fp);

	header.subchunk_format.chunk = MAKE_FOURCC('f', 'm', 't', ' ');
	fwrite(&header.subchunk_format.chunk, sizeof(char), sizeof(header.subchunk_format.chunk), fp);
	header.subchunk_format.subchunk_size = 16; // PCM编码不需要ex_size
	fwrite(&header.subchunk_format.subchunk_size, sizeof(char), sizeof(header.subchunk_format.subchunk_size), fp);
	header.subchunk_format.audio_format = 1;
	fwrite(&header.subchunk_format.audio_format, sizeof(char), sizeof(header.subchunk_format.audio_format), fp);
	header.subchunk_format.channels = channels;
	fwrite(&header.subchunk_format.channels, sizeof(char), sizeof(header.subchunk_format.channels), fp);
	header.subchunk_format.sample_rate = sample_rate;
	fwrite(&header.subchunk_format.sample_rate, sizeof(char), sizeof(header.subchunk_format.sample_rate), fp);
	header.subchunk_format.byte_rate = sample_rate*channels*sample_bits / 8;
	fwrite(&header.subchunk_format.byte_rate, sizeof(char), sizeof(header.subchunk_format.byte_rate), fp);
	header.subchunk_format.block_align = channels*sample_bits / 8;
	fwrite(&header.subchunk_format.block_align, sizeof(char), sizeof(header.subchunk_format.block_align), fp);
	header.subchunk_format.bits_per_sample = sample_bits;
	fwrite(&header.subchunk_format.bits_per_sample, sizeof(char), sizeof(header.subchunk_format.bits_per_sample), fp);
	// header.subchunk_format.ex_size = 0;
	// fwrite(&header.subchunk_format.ex_size, sizeof(char), sizeof(header.subchunk_format.ex_size), fp);

	header.subchunk_data.chunk = MAKE_FOURCC('d', 'a', 't', 'a');
	fwrite(&header.subchunk_data.chunk, sizeof(char), sizeof(header.subchunk_data.chunk), fp);
	header.subchunk_data.subchunk_size = pcm_file_size;
	fwrite(&header.subchunk_data.subchunk_size, sizeof(char), sizeof(header.subchunk_data.subchunk_size), fp);
}

int PCM2Wave(const char* pcm_file, const char* wav_file, int sample_rate, int sample_bits, int channels)
{
	FILE *fp_pcm = nullptr;
	errno_t err = fopen_s(&fp_pcm, pcm_file, "rb");
	if (err != 0) {
		printf("open file failed, %s\n", pcm_file);
		return -1;
	}

	FILE *fp_wav = nullptr;
	err = fopen_s(&fp_wav, wav_file, "wb");
	if (err != 0) {
		printf("open file failed, %s\n", wav_file);
		return -1;
	}

	// Get pcm file size
	fseek(fp_pcm, 0L, SEEK_END);
	long pcm_size = ftell(fp_pcm);
	fseek(fp_pcm, 0L, SEEK_SET);
	printf("pcm file size:%ld\n", pcm_size);

	// Write wave header
	write_wave_header(fp_wav, (int)pcm_size, sample_rate, sample_bits, channels);

	// Copy PCM data
	while (true) {
		uint8_t buff[2048];
		int read_cnt = fread_s(buff, 2048, sizeof(uint8_t), 2048, fp_pcm);
		if (read_cnt <= 0) {
			break;
		}

		fwrite(buff, sizeof(uint8_t), read_cnt, fp_wav);
	}

	fclose(fp_wav);
	fclose(fp_pcm);
	printf("convert %s to %s success!\n", pcm_file, wav_file);
    return 0;
}

int main(int argc, char* argv[])
{
    if(argc < 6){
        print_usage(argv);
        return -1;
    }

	const char* pcm_file = argv[1];
	const char* wav_file = argv[2];

	int sample_rate = 0;
	if (sscanf_s(argv[3], "%d", &sample_rate) <= 0) {
		printf("invalid sample_rate: %s\n", argv[3]);
		return -1;
	}

	if (sample_rate != 8000 && sample_rate != 12000 && sample_rate != 16000 && \
		sample_rate != 24000 && sample_rate != 32000 && sample_rate != 44100 && sample_rate != 48000) {
		printf("invalid sample_rate: %s\n", argv[3]);
		return -1;
	}

	int sample_bits = 0;
	if (sscanf_s(argv[4], "%d", &sample_bits) <= 0) {
		printf("invalid sample_bits: %s\n", argv[4]);
		return -1;
	}

	if (sample_bits <= 0 || sample_bits % 8 != 0) {
		printf("invalid sample_bits: %s\n", argv[4]);
		return -1;
	}

	int channels = 0;
	if (sscanf_s(argv[5], "%d", &channels) <= 0) {
		printf("invalid channels: %s\n", argv[5]);
		return -1;
	}

	if (channels != 1 && channels != 2) {
		printf("invalid channels: %s\n", argv[5]);
		return -1;
	}

	printf("convert %s to %s, sample_rate:%s, sample_bits:%s, channels:%s\n", argv[1], argv[2], argv[3], argv[4], argv[5]);
	return PCM2Wave(pcm_file, wav_file, sample_rate, sample_bits, channels);
}
