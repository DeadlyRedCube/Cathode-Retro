#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace ButterworthIIR
{
  template <typename t_type>
  static constexpr t_type k_pi = t_type(3.1415926535897932384626433832795028841971);

  // SOS is a second-order section
  struct SOS
  {
    double b0;
    double b1;
    double b2;
    // a0 is assumed to be 1.0.
    double a1;
    double a2;
  };


  // SOS Processor takes a single second order section, and will process input (one input at a time).
  class SOSProcessor
  {
  public:
    SOSProcessor(const SOS &coeff)
      : coefficients(coeff)
      { }

    double Process(double input)
    {
      // Calculate our output value, then update our history for next time
      double out = input * coefficients.b0 + z1;
      z1 = input * coefficients.b1 - coefficients.a1 * out + z2;
      z2 = input * coefficients.b2 - coefficients.a2 * out;
      return out;
    }

    // Reset the history of this processor so you can start it over.
    void ResetHistory()
      { z1 = z2 = 0.0; }

  private:
    // The actual second order section coefficients
    SOS coefficients;

    // Some history values
    double z1 = 0.0;
    double z2 = 0.0;
  };

  class Filter
  {
  public:
    // Create a low-pass Butterworth filter with at a given order with a given normalized frequency (that is, the frequency has been divided by the nyquist of your sample rate)
    static Filter CreateLowPass(int order, float normalizedFreq)
      { return Create(order, normalizedFreq, FilterType::LowPass); }

    // Create a low-pass Butterworth filter with at a given order with a given frequency and sample rate
    static Filter CreateLowPass(int order, float freq, float sampleRate)
      { return Create(order, freq / (sampleRate / 2.0f), FilterType::LowPass); }

    // Create a high-pass Butterworth filter with at a given order with a given normalized frequency
    static Filter CreateHighPass(int order, float normalizedFreq)
      { return Create(order, normalizedFreq, FilterType::HighPass); }

    // Create a high-pass Butterworth filter with at a given order with a given frequency and sample rate
    static Filter CreateHighPass(int order, float freq, float sampleRate)
      { return Create(order, freq / (sampleRate / 2.0f), FilterType::HighPass); }

    // Create a band pass Butterworth filter with at a given order with a given pair of normalized frequencies
    static Filter CreateBandPass(int order, float normalizedFreq1, float normalizedFreq2)
      { return Create(order, normalizedFreq1, normalizedFreq2, FilterType::BandPass); }

    // Create a band pass Butterworth filter with at a given order with a given pair of frequencies and sample rate
    static Filter CreateBandPass(int order, float freq1, float freq2, float sampleRate)
      { return Create(order, freq1 / (sampleRate / 2.0f), freq2 / (sampleRate / 2.0f), FilterType::BandPass); }

    // Create a band stop Butterworth filter with at a given order with a given pair of normalized frequencies
    static Filter CreateBandStop(int order, float normalizedFreq1, float normalizedFreq2)
      { return Create(order, normalizedFreq1, normalizedFreq2, FilterType::BandStop); }

    // Create a band stop Butterworth filter with at a given order with a given pair of frequencies and sample rate
    static Filter CreateBandStop(int order, float freq1, float freq2, float sampleRate)
      { return Create(order, freq1 / (sampleRate / 2.0f), freq2 / (sampleRate / 2.0f), FilterType::BandStop); }

    // Reset the history of this butterworth filter
    void ResetHistory()
    {
      for (auto &processor: m_processors)
      {
        processor.ResetHistory();
      }
    }

    Filter &Append(const Filter &other)
    {
      for (auto &&proc : other.m_processors)
      {
        this->m_processors.push_back(proc);
      }

      return *this;
    }

    // Run a single double-precision sample through the filter to get a single sample of output.
    double Process(double input)
    {
      // There are many ways to optimize this code, this is just the basic "it works" way to do it. For me the hard part was getting the code to GENERATE the filters
      //  working.
      double result = input;
      for (auto &processor: m_processors)
      {
        result = processor.Process(result);
      }

      return result;
    }

    void MeasureLatency()
    {
      f64 maxOutput = Process(1.0);
      latency = 0;

      for (u32 i = 0; i < 100; i++)
      {
        f64 output = Process(0.0);
        if (output > maxOutput)
        {
          maxOutput = output;
          latency = i + 1;
        }
      }
    }

    // Run a single single-precision sample through the filter to get a single sample of output.
    float Process(float input)
      { return float(Process(double(input))); }

    template <typename AllocA, typename AllocB>
    void Process(const std::vector<float, AllocA> &samplesIn, std::vector<float, AllocB> *pSamplesOut)
    { 
      for (u32 i = 0; i < latency; i++)
      {
        Process(samplesIn[i]);
      }

      for (u32 i = 0; i < samplesIn.size() - latency; i++) 
      { 
        (*pSamplesOut)[i] = Process(samplesIn[i + latency]); 
      } 

      for (u32 i = 0; i < latency; i++)
      {
        (*pSamplesOut)[samplesIn.size() - latency + i] = Process(0.0f);
      }
    }

    // Run an array of single-precision samples through the filter and store the results in place.
    void ProcessInPlace(std::vector<float> &samples)
    { 
      for (auto &sample : samples) 
      { 
        sample = Process(sample); 
      } 
    }

    // Run an array of double-precision samples through the filter and store the results in place.
    void ProcessInPlace(std::vector<double> &samples)
    { 
      for (auto &sample : samples) 
      { 
        sample = Process(sample); 
      } 
    }

  protected:
    enum class FilterType
    {
      LowPass,
      HighPass,
      BandPass,
      BandStop,
    };

    static Filter Create(int order, float normalizedFreq, FilterType type)
      { return Create(order, normalizedFreq, normalizedFreq, type); }

    static Filter Create(int order, float normalizedFreq1, float normalizedFreq2, FilterType type)
    {
      // Need to pre-warp the frequency for the analog -> digital conversion
      double freq1 = 2.0 * std::tan(k_pi<double> * normalizedFreq1 / 2.0);
      double freq2 = 2.0 * std::tan(k_pi<double> * normalizedFreq2 / 2.0);

      if (freq2 < freq1)
      {
        // Ensure the frequencies are in the expected order
        std::swap(freq1, freq2);
      }

      double centerFreq = freq1;
      double bandwidth = 0.0;

      // Butterworth filter has N poles and no zeros. The poles all live right on the unit circle.
      constexpr std::complex<double> k_i(0, 1);
      const std::complex<double> k_iPi = k_i * k_pi<double>;
      std::vector<std::complex<double>> zeros;
      std::vector<std::complex<double>> polePairs; // Each entry here is a pole and its implied conjugate

      // Odd-order filters (3, 5, etc) 
      bool hasOddPole = (order & 1) != 0;
      std::complex<double> oddPole = -1;

      // The poles (rounded down) get calculated evenly spaced in the left side of the space (negative real side).
      for (int i = 1; i < order; i += 2)
      {
        polePairs.push_back(std::exp(k_iPi * (double(i) / double(2 * order) + 0.5)));
      }

      double gain = 1.0;

      switch (type)
      {
      case FilterType::LowPass:
        // Generating a low-pass butterworth is the easiest, you just multiply the poles by the cutoff frequency
        for (auto &polePair : polePairs)
        {
          polePair *= centerFreq;
        }

        if (hasOddPole)
        {
          oddPole *= centerFreq;
        }

        // The gain simply adjusts by the center frequency power.
        gain = std::pow(centerFreq, order);
        break;

      case FilterType::HighPass:
        {
          // High pass filters are a little more complex in that we have to flip the pole's value (via a complex divide) while multiplying in the center frequency
          for (auto &polePair : polePairs)
          {
            polePair = std::complex<double>(centerFreq) / polePair;

            // High pass has zeros at 0 (one per pole, which means two per pole pair)
            zeros.push_back(0.0);
            zeros.push_back(0.0);
          }

          if (hasOddPole)
          {
            oddPole = std::complex<double>(centerFreq) / oddPole;
            zeros.push_back(0.0);
          }

          // No adjusting the gain for a high pass filter.
        }
    
        break;

      case FilterType::BandPass:
        {
          // Band passes will end up with 2*order poles (so "order" pole pairs)

          // Calculate the center frequency and the bandwidth
          centerFreq = std::sqrt(freq1 * freq2);
          bandwidth = freq2 - freq1;

          std::vector<std::complex<double>> origPolePairs = std::move(polePairs);
          polePairs = {};

          for (auto &polePair : origPolePairs)
          {
            // each pole gets modified by the following formula (leaving us still with these values and their conjugates, so the conjugate pairing here still works)
            auto a = 0.5 * polePair * bandwidth;
            auto b = 0.5 * std::sqrt(double(bandwidth * bandwidth) * (polePair * polePair) - 4 * centerFreq * centerFreq);
            polePairs.push_back(a + b);
            polePairs.push_back(a - b);
          }

          // The odd pole itself becomes now a pair (new pole and conjugate), so stop factoring the odd pole in.
          if (hasOddPole)
          {
            polePairs.push_back(0.5 * bandwidth * oddPole + 0.5 * std::sqrt(double(bandwidth * bandwidth) * (oddPole * oddPole) - 4 * centerFreq * centerFreq));
            hasOddPole = false;
          }

          for (int i = 0; i < order; i++)
          {
            // Band pass has zeros at ... well, zero. One zero zero per order.
            zeros.push_back(0.0);
          }

          // Similar to a low-pass, the gain here gets multiplied by a power of the bandwidth.
          gain = std::pow(bandwidth, order);
        }
        break;

      case FilterType::BandStop:
        {
          // Band stop filters work similarly to band pass filters, except it's a divide by the pole instead of a multiply.

          // Calculate the center frequency and the bandwidth
          centerFreq = std::sqrt(freq1 * freq2);
          bandwidth = freq2 - freq1;

          std::vector<std::complex<double>> origPolePairs = std::move(polePairs);
          polePairs = {};
          for (auto &polePair : origPolePairs)
          {
            auto a = 0.5 * bandwidth / polePair;
            auto b = 0.5 * std::sqrt(double(bandwidth * bandwidth) / (polePair * polePair) - 4 * centerFreq * centerFreq);
            polePairs.push_back(a + b);
            polePairs.push_back(a - b);
          }

          // The odd pole itself becomes a conjugate pair so it's no longer an odd set of poles.
          if (hasOddPole)
          {
            polePairs.push_back(0.5 * bandwidth / oddPole + 0.5 * std::sqrt(double(bandwidth * bandwidth) / (oddPole * oddPole) - 4 * centerFreq * centerFreq));
            hasOddPole = false;
          }

          for (int i = 0; i < order; i++)
          {
            // We should have twice as many zeros as the order of our filter, each a pure imaginary number and its opposite, alternating.
            zeros.push_back({0.0,  centerFreq});
            zeros.push_back({0.0, -centerFreq});
          }

          // Like the high pass, there's no adjustment of the gain for a band stop filter.
        }
        break;
      }

      // Now we have to map from the s-plane to the z-plane, so it's bilinear transform time yay
      for (auto &zero : zeros)
      {
        // Adjust the gain before modifying the zero
        gain *= std::abs(2.0 - zero);

        zero = (2.0 + zero) / (2.0 - zero);
      }

      for (auto &polePair : polePairs)
      {
        // Each polePair is a conjugate pair so factor each length into the gain twice
        double gainMod = std::abs(2.0 - polePair);
        gain /= gainMod*gainMod;

        polePair = (2.0 + polePair) / (2.0 - polePair);
      }

      if (hasOddPole)
      {
        gain /= std::abs(2.0 - oddPole);
        oddPole = (2.0 + oddPole) / (2.0 - oddPole);
      }

      // Pad out the zero array with -1s (zeros at infinity, basically) until there are as many as there are poles (not pole pairs)
      size_t zeroCount = polePairs.size() * 2 + (hasOddPole ? 1 : 0);
      for (size_t i = zeros.size(); i < zeroCount; i++)
      {
        zeros.push_back(-1.0);
      }

      Filter filter;
      // Finally, generate second order sections from the poles
      if (hasOddPole)
      {
        // Odd filters have a special second order section that's really a first-order section.
        filter.m_processors.push_back(SOS{
          1.0 * gain, 
          -zeros[zeros.size() - 1].real() * gain,
          0.0 * gain, 
          -oddPole.real(), 
          0.0});

        gain = 1.0; // Only the first SOS in the set needs to have the gain factored in, so set the gain to 1 so it doesn't affect any more
      }

      for (unsigned int i = 0; i < polePairs.size(); i++)
      {
        auto poleA = polePairs[i];
        auto zeroA = zeros[i*2 + 0];
        auto zeroB = zeros[i*2 + 1];
        filter.m_processors.push_back(SOS{
          1.0 * gain, 
          -(zeroA + zeroB).real() * gain,
          (zeroA * zeroB).real() * gain,
          -2.0 * poleA.real(),  // This is technically -(pole + conj(pole)), which simplifies down to -2*real
          poleA.real()*poleA.real() + poleA.imag()*poleA.imag()}); // This is (pole * conj(pole))

        gain = 1.0; // Only the first SOS in the set needs to have the gain factored in, so set the gain to 1 so it doesn't affect any more
      }

      return filter;
    }

    std::vector<SOSProcessor> m_processors;
    u32 latency = 0;
  };
}