#include <cmath>

#include "curves.hpp"


namespace alicevision {
    namespace image {

        // Tone curve according to Adobe's reference implementation
        // values in 0xffff space
        void AdobeToneCurve::Apply(float& ir, float& ig, float& ib) const
        {

            const float r = std::max<float>(0.0, std::min<float>(65535.0, ir));
            const float g = std::max<float>(0.0, std::min<float>(65535.0, ig));
            const float b = std::max<float>(0.0, std::min<float>(65535.0, ib));

            if (r >= g) {
                if (g > b) {
                    RGBTone(r, g, b);     // Case 1: r >= g >  b
                }
                else if (b > r) {
                    RGBTone(b, r, g);     // Case 2: b >  r >= g
                }
                else if (b > g) {
                    RGBTone(r, b, g);     // Case 3: r >= b >  g
                }
                else {                           // Case 4: r == g == b
                    r = lutToneCurve[r];
                    g = lutToneCurve[g];
                    b = g;
                }
            }
            else {
                if (r >= b) {
                    RGBTone(g, r, b);     // Case 5: g >  r >= b
                }
                else if (b > g) {
                    RGBTone(b, g, r);     // Case 6: b >  g >  r
                }
                else {
                    RGBTone(g, b, r);     // Case 7: g >= b >  r
                }
            }

            //setUnlessOOG(ir, ig, ib, r, g, b);
            if (!(ir < 0.0 || ir > 65535.0) || !(ig < 0.0 || ig > 65535.0) || !(ib < 0.0 || ib > 65535.0)) {
                ir = r;
                ig = g;
                ib = b;
            }

        }


        inline void AdobeToneCurve::RGBTone(float& maxval, float& medval, float& minval) const
        {
            float minvalold = minval, medvalold = medval, maxvalold = maxval;

            maxval = lutToneCurve[maxvalold];
            minval = lutToneCurve[minvalold];
            medval = minval + ((maxval - minval) * (medvalold - minvalold) / (maxvalold - minvalold));
        }

        void ToneCurve::Reset()
        {
            lutToneCurve.reset();
        }

        // Fill a LUT with X/Y, ranged 0xffff
        void ToneCurve::Set(const Curve& pCurve, float gamma)
        {
            lutToneCurve(65536);

            if (gamma <= 0.0 || gamma == 1.) {
                for (int i = 0; i < 65536; i++) {
                    lutToneCurve[i] = (float)pCurve.getVal(float(i) / 65535.f) * 65535.f;
                }

            }
        }


        Curve::Curve() : N(0), ppn(0), x(nullptr), y(nullptr), mc(0.0), mfc(0.0), msc(0.0), mhc(0.0), hashSize(1000 /* has to be initialized to the maximum value */), ypp(nullptr), x1(0.0), y1(0.0), x2(0.0), y2(0.0), x3(0.0), y3(0.0), firstPointIncluded(false), increment(0.0), nbr_points(0) {}


        DiagonalCurve::DiagonalCurve(const std::vector<double>& p, int poly_pn)
        {

            ppn = poly_pn > 65500 ? 65500 : poly_pn;

            if (ppn < 500) {
                hashSize = 100;    // Arbitrary cut-off value, but multiple of 10
            }

            if (ppn < 50) {
                hashSize = 10;    // Arbitrary cut-off value, but multiple of 10
            }

            if (p.size() < 3) {
                kind = DCT_Empty;
            }
            else {
                bool identity = true;
                kind = (DiagonalCurveType)p[0];

                if (kind == DCT_Linear || kind == DCT_Spline || kind == DCT_NURBS || kind == DCT_CatumullRom) {
                    N = (p.size() - 1) / 2;
                    x = new double[N];
                    y = new double[N];
                    int ix = 1;

                    for (int i = 0; i < N; i++) {
                        x[i] = p[ix++];
                        y[i] = p[ix++];

                        if (std::fabs(x[i] - y[i]) >= 0.000009) {
                            // the smallest possible difference between x and y curve point values is ~ 0.00001
                            // checking against >= 0.000009 is a bit saver than checking against >= 0.00001
                            identity = false;
                        }
                    }

                    if (x[0] != 0.0 || x[N - 1] != 1.0)
                        // Special (and very rare) case where all points are on the identity line but
                        // not reaching the limits
                    {
                        identity = false;
                    }

                    if (x[0] == 0.0 && x[1] == 0.0)
                        // Avoid crash when first two points are at x = 0 (git Issue 2888)
                    {
                        x[1] = 0.01;
                    }

                    if (x[0] == 1.0 && x[1] == 1.0)
                        // Avoid crash when first two points are at x = 1 (100 in gui) (git Issue 2923)
                    {
                        x[0] = 0.99;
                    }

                    if (!identity) {
                        if (kind == DCT_Spline && N > 2) {
                            spline_cubic_set();
                        }
                        else {
                            kind = DCT_Linear;
                        }
                    }
                }

                if (identity) {
                    kind = DCT_Empty;
                }
            }
        }

        DiagonalCurve::~DiagonalCurve()
        {

            delete[] x;
            delete[] y;
            delete[] ypp;
            poly_x.clear();
            poly_y.clear();
        }

        void DiagonalCurve::spline_cubic_set()
        {

            double* u = new double[N - 1];
            delete[] ypp;      // TODO: why do we delete ypp here since it should not be allocated yet?
            ypp = new double[N];

            ypp[0] = u[0] = 0.0;    /* set lower boundary condition to "natural" */

            for (int i = 1; i < N - 1; ++i) {
                double sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
                double p = sig * ypp[i - 1] + 2.0;
                ypp[i] = (sig - 1.0) / p;
                u[i] = ((y[i + 1] - y[i])
                    / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]));
                u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
            }

            ypp[N - 1] = 0.0;

            for (int k = N - 2; k >= 0; --k) {
                ypp[k] = ypp[k] * ypp[k + 1] + u[k];
            }

            delete[] u;
        }

        double DiagonalCurve::getVal(double t) const
        {

            switch (kind) {

            case DCT_Linear:
            case DCT_Spline:
            {
                // values under and over the first and last point
                if (t > x[N - 1]) {
                    return y[N - 1];
                }
                else if (t < x[0]) {
                    return y[0];
                }

                // do a binary search for the right interval:
                unsigned int k_lo = 0, k_hi = N - 1;

                while (k_hi > 1 + k_lo) {
                    unsigned int k = (k_hi + k_lo) / 2;

                    if (x[k] > t) {
                        k_hi = k;
                    }
                    else {
                        k_lo = k;
                    }
                }

                double h = x[k_hi] - x[k_lo];

                // linear
                if (kind == DCT_Linear) {
                    return y[k_lo] + (t - x[k_lo]) * (y[k_hi] - y[k_lo]) / h;
                }
                // spline curve
                else { // if (kind==Spline) {
                    double a = (x[k_hi] - t) / h;
                    double b = (t - x[k_lo]) / h;
                    double r = a * y[k_lo] + b * y[k_hi] + ((a * a * a - a) * ypp[k_lo] + (b * b * b - b) * ypp[k_hi]) * (h * h) * 0.1666666666666666666666666666666;
                    return (r > 0.0 ? (r < 1.0 ? r : 1.0) : 0.0);
                }

                break;
            }

            case DCT_Empty:
            default:
                // all other (unknown) kind
                return t;
            }
        }
    }
}