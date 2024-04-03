/*
    Apple-II-Cassette-Recover (a2cassre)

    Copyright © 2019, Christopher Alan Mosher, Shelton, CT, USA. <cmosher01@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY, without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <SDL.h>
#include <cmath>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>



static const float MOVEMENT_THRESHOLD = 0.0005f;
static const int Q = 20;
// static const bool SLOPE = true;
static const bool SLOPE = false;

static std::int_fast8_t slope(const float prev, const float curr) {
    if (abs(curr-prev) < MOVEMENT_THRESHOLD) {
        return 0;
    }
    return prev < curr ? +1 : -1;
}

static double zero(const double t0, const double v0, const double t1, const double v1) {
    const double m = (v1-v0)/(t1-t0);
    const double b = v0-m*t0;
    const double z = -b/m;
    // std::printf("%18.8f <== %18.8f,%18.8f;%18.8f,%18.8f\n", z, t0, v0, t1, v1);
    return z;
}

/*
 * 0 [13.5] 20 25 [37.5] 50 [57.5] 65 [72.5]
 * (Translate 20 to 25)
 */
static std::uint32_t filter(std::uint32_t d) {
    if (d <= 13u) {
        return 0u;
    }
    if (d <= 37u) {
        return 25u;
    }
    if (d <= 57u) {
        return 50u;
    }
    if (d <= 72u) {
        return 65u;
    }
    return 1000+d;
}

static uint32_t filter_spikes(const uint32_t dm1, const uint32_t d, const uint32_t dp1) {
    return (dm1 == 0 && dp1 == 0) ? 0 : d;
}

static std::uint32_t safe_get(std::vector<std::uint32_t>& raw, std::int_fast32_t i) {
    if (i < 0 || raw.size() <= i) {
        return 0;
    }

    return raw[i];
}

static bool sync(std::vector<std::uint32_t>& raw, std::int_fast32_t i) {
    return safe_get(raw,i) == 25 && safe_get(raw,i+1) == 25 && (
                (safe_get(raw,i+2)==25 && safe_get(raw,i+3)==25)||
                (safe_get(raw,i+2)==50 && safe_get(raw,i+3)==50)
            ) && (
                (safe_get(raw,i+4)==25 && safe_get(raw,i+5)==25)||
                (safe_get(raw,i+4)==50 && safe_get(raw,i+5)==50)
            );
}

enum state_t { UNKNOWN, HEADER_PENDING, HEADER, SYNC, FIRST_HALF_BIT, SECOND_HALF_BIT, END };

static std::vector<std::uint32_t> analyze_fsm(std::vector<std::uint32_t>& raw) {
    std::vector<std::uint32_t> clean;
    enum state_t state = UNKNOWN;
    std::int_fast32_t i = 0; // position in raw vector


    const std::uint_fast32_t C_HEADER_MIN = 17;
    std::uint_fast32_t c_header = 0;
    bool next = true;
    // std::printf("analyze_fsm: i=%d, size=%lu\n", i, raw.size());
    while (state != END) {
        std::uint32_t dm1 = safe_get(raw, i-1);
        std::uint32_t d = safe_get(raw, i);
        // std::printf("%d ", d);
        // if (!(i%50)) { std::printf("\n"); }
        std::uint32_t d1 = safe_get(raw, i+1);
        std::uint32_t d2 = safe_get(raw, i+2);
        std::uint32_t d3 = safe_get(raw, i+3);

        switch (state) {
        case UNKNOWN: {
            if (d == 65) {
                state = HEADER_PENDING;
                c_header = 0;
            }
        }
        break;
        case HEADER_PENDING: {
            if (d == 65) {
                ++c_header;
            } else {
                state = UNKNOWN;
            }
            if (C_HEADER_MIN <= c_header) {
                state = HEADER;
            }
        }
        break;
        case HEADER:
            if (sync(raw,i)) {
                state = SYNC;
            } else if (d == 50 && sync(raw,i+1)) {
                /* sometimes the final header wave gets cut a little short */
                d = 65;
            } else if (d != 65) {
                if (d1 == 65 || d2 == 65 || d3 == 65) {
                    d = 65;
                } else {
                    state = UNKNOWN;
                }
            }
        break;
        case SYNC: {
            // std::printf("\n======================\n", i);
            // std::printf("BEGIN: i=%d\n", i);
            // std::printf("======================\n", i);
            clean.push_back(65); // represent entire header as one 65 entry
            clean.push_back(20); // synch bit (first half)
            clean.push_back(25); // synch bit (second half)
            state = FIRST_HALF_BIT;
        }
        break;
        case FIRST_HALF_BIT: {
            if (1000 <= d) {
                // std::printf("\n======================\n", i);
                // std::printf("END: i=%d\n", i);
                // std::printf("======================\n", i);
                state = UNKNOWN;
            } else {
                if (d == 0 || d == 65) {
                    if (d1 == 25 || d1 == 50) {
                        d = d1;
                    } else {
                        d = 0; // lost bit
                    }
                }
                state = SECOND_HALF_BIT;
            }
        }
        break;
        case SECOND_HALF_BIT: {
            if (1000 <= d) {
                // std::printf("\n======================\n", i);
                // std::printf("END: i=%d\n", i);
                // std::printf("======================\n", i);
                state = UNKNOWN;
            } else {
                if (d == 0 || d == 65) {
                    if (dm1 == 25 || dm1 == 50) {
                        d = dm1;
                    } else {
                        d = 0; // lost bit
                    }
                }
                clean.push_back(d);
                clean.push_back(d);
                state = FIRST_HALF_BIT;
            }
        }
        break;
        }
        if (next) {
            ++i;
            if (raw.size() <= i) {
                state = END;
            }
        }
    }
    // std::printf("\n======================\n", i);
    // std::printf("\n======================\n", i);
    // std::printf("\n======================\n", i);
    return clean;
}

void out_cycle(const std::uint_fast32_t d, const bool positive, std::ofstream &out) {
    const float pi = acos(-1);
    for (std::uint_fast8_t s = 0; s < d/5; ++s) {
        float x = sin(pi/2 + 5*pi*s/d);
        if (!positive) {
            x = -1.0f*x;
        }
        x = round(128+128*x);
        if (x > 255.0f) {
            x = 255.0f;
        }
        if (x < 0.0f) {
            x = 0.0f;
        }
        std::uint8_t bx = x;
        out.write((char*)&bx, 1);
    }
}

void out_wave(const std::vector<std::uint32_t>& ds, const std::string& to_file) {
    std::ofstream out(to_file, std::ios::binary);



    std::uint32_t c_sample = 0;
    for (std::uint32_t i = 0; i < ds.size(); ++i) {
        if (ds[i] == 65) {
            c_sample += (2040+204100/2);
        } else {
            c_sample += ds[i]/5;
        }
    }
    c_sample += 2040;

    std::uint32_t longVal;
    std::uint16_t wordVal;

    out.write("RIFF", 4);
    out.write((char*)&(longVal = 36+c_sample), 4);
    out.write("WAVE", 4);



    out.write("fmt ", 4);
    out.write((char*)&(longVal = 16), 4);

    out.write((char*)&(wordVal = 1), 2); // PCM
    out.write((char*)&(wordVal = 1), 2); // mono, one channel

    out.write((char*)&(longVal = 20410), 4); // samples per second
    out.write((char*)&(longVal = 20410), 4); // byte rate (same)
    out.write((char*)&(wordVal = 1), 2); // alignment
    out.write((char*)&(wordVal = 8), 2); // bits per sample



    out.write("data", 4);
    out.write((char*)&(longVal = c_sample), 4);

    bool positive = false;
    for (std::uint32_t i = 0; i < ds.size(); ++i) {
        const std::uint_fast8_t d = ds[i];
        if (d == 65) {
            std::uint8_t silence = 128;
            for (int xxx = 0; xxx < 2040; ++xxx) {
                out.write((char*)&silence, 1);
                positive = !positive;
            }
            for (int xxx = 0; xxx < 204100/(2*65/5); ++xxx) {
                out_cycle(d, positive, out);
                positive = !positive;
            }
            positive = !positive;
        } else {
            out_cycle(d, positive, out);
        }
        positive = !positive;
    }
    std::uint8_t silence = 128;
    for (int xxx = 0; xxx < 2040; ++xxx) {
        out.write((char*)&silence, 1);
    }



    out.flush();
    out.close();
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::printf("Usage: %s input.wav output.wav\n", argv[0]);
        std::printf("Recovers data from Apple ][ cassette tape image input.wav,\n");
        std::printf("and writes it to output.wav\n");
        return 1;
    }

    if (SDL_Init(0) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec wav_spec;
    Uint32 wav_length;
    Uint8 *wav_buffer;
    if (SDL_LoadWAV(argv[1], &wav_spec, &wav_buffer, &wav_length) == nullptr) {
        SDL_Log("Could not open file: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("Loaded WAVE file:\n");
    std::printf("    buffer size: %d bytes\n", wav_length);
    std::printf("    sample rate: %dHz\n", wav_spec.freq);
    std::printf("    sample datatype: %04X\n", wav_spec.format);
    std::printf("    channels: %d\n", wav_spec.channels);
    std::printf("    silence value: %d\n", wav_spec.silence);
    std::printf("    sample count: %d\n", wav_spec.samples);

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq, AUDIO_F32, 1, 102048) < 0) {
        SDL_Log("Unable to convert WAV: %s", SDL_GetError());
        return 1;
    }
    cvt.len = wav_length;
    cvt.buf = reinterpret_cast<Uint8*>(std::malloc(cvt.len * cvt.len_mult));
    std::memcpy(cvt.buf, wav_buffer, cvt.len);
    SDL_FreeWAV(wav_buffer);
    SDL_ConvertAudio(&cvt);
    // cvt.buf has cvt.len_cvt bytes of converted data now.
    const float *const input = reinterpret_cast<float*>(cvt.buf);
    const std::uint_fast32_t input_siz = cvt.len_cvt/4u;
    if (0) {
        int m = 0;
        for (int i = 0; i < input_siz; ++i) {
            std::printf("%f ", input[i]);
            if (!(++m % 50)) {
                std::printf("\n");
            }
        }
    }


    std::vector<std::uint32_t> cycle;

    if (SLOPE) { // slope-change-based algorithm
        std::int_fast8_t slope_was = 0;
        std::uint_fast32_t i_was = 0;
        int q = 0;
        for (std::uint_fast32_t i = 1; i < input_siz-1; ++i) {
            std::int_fast8_t slope_is = slope(input[i-1], input[i]);
            const std::uint32_t a = i-i_was;
            if (Q < ++q && slope_is != 0 && slope_is != slope_was && (11u < a)) {
                q = 0;
                cycle.push_back(a);
                i_was = i;
                slope_was = slope_is;
            }
        }
    } else { // zero-crossing-based algorithm
        const double E = 0.0;
        const double M = (1.0E5/102048.0);
        double t = 0.0;
        for (std::uint_fast32_t i = 1; i < input_siz; ++i) {
            if ((input[i-1] < E && E < input[i]) || (input[i] < E && E < input[i-1])) {
                double p = t;
                t = zero((i-1)*M, input[i-1], i*M, input[i]);
                cycle.push_back(std::round(t-p));
            }
        }
    }

    if (0) {
        int m = 0;
        for (std::uint_fast32_t i = 0; i < cycle.size(); ++i) {
            std::uint32_t x = cycle[i];
            std::printf("%d ", x);
            if (!(++m % 64)) {
                std::printf("\n");
            }
        }
    }

    std::vector<std::uint32_t> cycle1;
    for (std::uint_fast32_t i = 0; i < cycle.size(); ++i) {
        cycle1.push_back(filter(cycle[i]));
    }

    std::vector<std::uint32_t> cycle2;
    cycle2.push_back(cycle1[0]);
    for (std::uint_fast32_t i = 1; i < cycle1.size()-1; ++i) {
        cycle2.push_back(filter_spikes(cycle1[i-1],cycle1[i],cycle1[i+1]));
    }
    cycle2.push_back(cycle1[cycle1.size()-1]);

    std::vector<std::uint32_t> cycle3;
    cycle3.push_back(cycle2[0]);
    for (std::uint_fast32_t i = 1; i < cycle2.size(); ++i) {
        if (!(cycle2[i] == 0 && cycle2[i-1] == 0)) {
            cycle3.push_back(cycle2[i]);
        }
    }

    std::vector<std::uint32_t> cycle4 = analyze_fsm(cycle3);

    // dump:
    if (0)
    {
        int cm = 64;
        int im = 0;
        for (std::uint_fast32_t i = 0; i < cycle4.size(); ++i) {
            if (cycle4[i] == 65) {
                std::printf("\n");
                im = 0;
                cm = 3;
            }
            std::printf("%d ", cycle4[i]);
            if (cm <= ++im) {
                std::printf("\n");
                cm = 64;
                im = 0;
            }
        }
        std::printf("\n");
    }

    // dump hex:
    if (1)
    {
        int NUDGE = 0;

        enum {START, SKIP, CAPTURE};

        int state = START;
        if (NUDGE) {
            state = CAPTURE;
        }
        int skip = 0;
        int c = cycle4.size();
        if (NUDGE) {
            c = 72+4*64*8*2;
        }
        int i = 0;
        if (NUDGE) {
            i = 72;
        }
        int cap = 0;
        int xbyt = 0;
        int chc = 0;
        while (i < c) {
            int xbit = cycle4[NUDGE*2+i++];
            switch (state) {
                case START: {
                    if (xbit == 65) {
                        skip = 2;
                        state = SKIP;
                    }
                }
                break;
                case SKIP: {
                    if (--skip <= 0) {
                        std::printf("\n[SKIP HEADER]\n");
                        state = CAPTURE;
                        cap = 0;
                        chc = 0;
                    }
                }
                break;
                case CAPTURE: {
                    if (xbit == 65) {
                        skip = 2;
                        state = SKIP;
                    } else {
                        if (cycle4[NUDGE*2+i] == xbit) {
                            ++i; // skip identical half cycle
                        } else {
                            printf("\nHALF CYCLE MISMATCH\n");
                        }
                        xbyt <<= 1;
                        if (xbit == 50) {
                            xbyt |= 1;
                        }
                        ++cap;
                        if (8 <= cap) {
                            cap = 0;
                            std::printf("%02x ", xbyt);
                            xbyt = 0;
                            if (64 <= ++chc) {
                                std::printf("\n");
                                chc = 0;
                            }
                        }
                    }
                }
                break;
                default: {

                }
            }
        }
    }

    out_wave(cycle4, argv[2]);

    return 0;
}
