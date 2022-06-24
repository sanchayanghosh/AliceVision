#pragma once

#include <vector>

namespace alicevision
{
    namespace image {

        class avLUTf
        {
        protected:
            // list of variables ordered to improve cache speed
            int maxs;
            float maxsf;
            float* data;
            unsigned int size;
            unsigned int upperBound;  // always equals size-1, parameter created for performance reason

        public:

            void operator ()(int s)
            {
                data = new float[s + 3];
                size = s;
                upperBound = size - 1;
                maxs = size - 2;
                maxsf = (float)maxs;
                clear();

            }

            avLUTf() = default;
            ~avLUTf() = default;

            avLUTf(int s)
            {
                // Add a few extra elements so [](vfloat) won't access out-of-bounds memory.
                // The routine would still produce the right answer, but might cause issues
                // with address/heap checking programs.
                data = new float[s + 3];
                size = s;
                upperBound = size - 1;
                maxs = size - 2;
                maxsf = (float)maxs;
                clear();
            }

            explicit avLUTf(const std::vector<float>& input) :
                maxs(input.size() - 2),
                maxsf(maxs),
                data(new float[input.size() + 3]), // Add a few extra elements so [](vfloat) won't access out-of-bounds memory.
                size(input.size()),
                upperBound(size - 1)
            {
                std::copy_n(input.begin(), input.size(), data);
            }

            float& operator[](int index) const
            {
                return data[(index < 0) ? 0 : ((index > upperBound) ? upperBound : index)];
            }

            float operator[](float index) const
            {
                if (index < 0.f) {
                    return data[0];
                }
                else if (index > maxsf) {
                    return data[upperBound];
                }
                else
                {
                    int idx = (int)index;
                    float diff = index - (float)idx;
                    float p1 = data[idx];
                    float p2 = data[idx + 1] - p1;
                    return (p1 + p2 * diff);
                }
            }

            void reset()
            {
                if (data) {
                    delete[] data;
                }

                data = nullptr;
                size = 0;
                upperBound = 0;
                maxs = 0;
                maxsf = 0.f;
            }

            void clear()
            {
                if (data && size) {
                    std::memset(data, 0, size * sizeof(float));
                }
            }

        };

        enum DiagonalCurveType {
            DCT_Empty = -1,     // Also used for identity curves
            DCT_Linear,         // 0
            DCT_Spline,         // 1
            DCT_Parametric,     // 2
            DCT_NURBS,          // 3
            DCT_CatumullRom,    // 4
            // Insert new curve type above this line
            DCT_Unchanged       // Must remain the last of the enum
        };

        class Curve
        {

            class HashEntry
            {
            public:
                unsigned short smallerValue;
                unsigned short higherValue;
            };
        protected:
            int N;
            int ppn;            // targeted polyline point number
            double* x;
            double* y;
            // begin of variables used in Parametric curves only
            double mc;
            double mfc;
            double msc;
            double mhc;
            // end of variables used in Parametric curves only
            std::vector<double> poly_x;     // X points of the faceted curve
            std::vector<double> poly_y;     // Y points of the faceted curve
            std::vector<double> dyByDx;
            std::vector<HashEntry> hash;
            unsigned short hashSize;        // hash table's size, between [10, 100, 1000]

            double* ypp;

            // Fields for the elementary curve polygonisation
            double x1, y1, x2, y2, x3, y3;
            bool firstPointIncluded;
            double increment;
            int nbr_points;

        public:
            Curve();
            virtual ~Curve() {};
            virtual double getVal(double t) const = 0;
        };

        class DiagonalCurve final : public Curve
        {

        protected:
            DiagonalCurveType kind;

            void spline_cubic_set();

        public:
            explicit DiagonalCurve(const std::vector<double>& points, int ppn = 1000);
            ~DiagonalCurve() override;

            double getVal(double t) const override;
        };

        class ToneCurve
        {
        public:
            ToneCurve() {};

            avLUTf lutToneCurve;  // 0xffff range

            virtual ~ToneCurve() {};

            void Reset();
            void Set(const Curve& pCurve, float gamma = 0);
        };

        class AdobeToneCurve final : public ToneCurve
        {
        public:
            AdobeToneCurve() {};
            ~AdobeToneCurve() {};
            void Apply(float& r, float& g, float& b) const;
        private:
            void RGBTone(float& r, float& g, float& b) const;
        };

    }
}
