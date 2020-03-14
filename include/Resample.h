/**
 * Resampling algo from http://en.wikipedia.org/wiki/Lanczos_resampling
 * Adapted by / copyright Evan Raskob <evan@flkr.com>, 2009-2019
 * Free to use with attribution to all original authors / MIT License
 * https://opensource.org/licenses/MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// example use to downsample an array BIG of floats from 1024 points to another array SMALL with 128 points:
// Resample(BIG, 1024, SMALL, 128);

template<int Radius>
static inline float Lanczos(float x)
{
	if (x == 0.0) return 1.0;
	if (x <= -Radius || x >= Radius) return 0.0;
	float pi_x = x * M_PI;
	return Radius * sin(pi_x) * sin(pi_x / Radius) / (pi_x * pi_x);
}

const int FilterRadius = 3;

static inline void Resample
(const float* source, size_t src_len,
 float* result, size_t dest_len)
{
    const float blur = 1.0;
    const float factor = dest_len / (float)src_len;

    float scale   = min(factor, 1.0f) / blur;
    float support = FilterRadius / scale;

    std::vector<float> contribution(min(src_len, 5+size_t(2*support)));
    /* 5 = room for rounding up in calculations of start, stop and support */

    if (support <= 0.5f) { support = 0.5f + 1E-12; scale = 1.0f; }

    for (size_t x=0; x<dest_len; ++x)
    {
        float center = (x+0.5f) / factor;
        size_t start = (size_t)max(center-support+0.5f, (float)0);
        size_t stop  = (size_t)min(center+support+0.5f, (float)src_len);
        float density = 0.0f;
        size_t nmax = stop-start;
        float s = start - center+0.5f;
        result[x] = 0;
        for (size_t n=0; n<nmax; ++n, ++s)
        {
            contribution[n] = Lanczos<FilterRadius> (s * scale);
            density += contribution[n];
            result[x] += source[start+n]*contribution[n];
        }
        if (density != 0.0 && density != 1.0)
        {
            /* Normalize. */
            result[x] /= density;
        }
    }
}