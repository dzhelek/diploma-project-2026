#include "algorithm_interface.h"
#include "algo_ascon.h"
#include "algo_tinyjambu.h"
#include "algo_schwaemm.h"

// Singleton instances — one per algorithm, allocated in static storage.
static AsconAlgorithm     s_ascon;
static TinyJAMBUAlgorithm s_tinyjambu;
static SCHWAEMMAlgorithm  s_schwaemm;

IAlgorithm* AlgorithmFactory(UartAlgorithm algo)
{
    switch (algo) {
        case ALGO_ASCON:      return &s_ascon;
        case ALGO_TINYJAMBU:  return &s_tinyjambu;
        case ALGO_SCHWAEMM:   return &s_schwaemm;
        default:              return nullptr;
    }
}
