#pragma once

void SHEval(unsigned int bandN, const float fX, const float fY, const float fZ, float *pSH);
void SHEval3(const float fX, const float fY, const float fZ, float *pSH);
void SHEval4(const float fX, const float fY, const float fZ, float *pSH);
void SHEval5(const float fX, const float fY, const float fZ, float *pSH);
void SHEval6(const float fX, const float fY, const float fZ, float *pSH);
void SHEval7(const float fX, const float fY, const float fZ, float *pSH);
void SHEval8(const float fX, const float fY, const float fZ, float *pSH);
void SHEval9(const float fX, const float fY, const float fZ, float *pSH);
void SHEval10(const float fX, const float fY, const float fZ, float *pSH);
void SHEval11(const float fX, const float fY, const float fZ, float *pSH);
void SHEval12(const float fX, const float fY, const float fZ, float *pSH);
void SHEval13(const float fX, const float fY, const float fZ, float *pSH);
void SHEval14(const float fX, const float fY, const float fZ, float *pSH);
void SHEval15(const float fX, const float fY, const float fZ, float *pSH);
void SHEval16(const float fX, const float fY, const float fZ, float *pSH);
void SHEval17(const float fX, const float fY, const float fZ, float *pSH);
void SHEval18(const float fX, const float fY, const float fZ, float *pSH);
void SHEval19(const float fX, const float fY, const float fZ, float *pSH);
void SHEval20(const float fX, const float fY, const float fZ, float *pSH);

void SHEval4D(const double fX, const double fY, const double fZ, double *pSH);
void SHEval11D(const double fX, const double fY, const double fZ, double *pSH);
void SHEval15D(const double fX, const double fY, const double fZ, double *pSH);

float ZHProduct3(const float fZ, float *ZHa, float *ZHb);
float ZHProductNormCos3(const float fZ, float *ZHa);
float ZHProduct11(const float fZ, float *ZHa, float *ZHb);
float ZHProductNormCos11(const float fZ, float *ZHa);

/// Compute the zonal weights associated to the given vector of directions
/// that converts dot powers (wd, w)^i to a ZH
/// pZHW : output array of weights (must be size order*order)
void ZonalWeights(int order, float *pZHW);

/// Vector group version
/// directions : input array of vec3, of size dirCount
/// pZHW : output array of weights, of size dirCount*(order^3), with order = (dirCount-1) / 2 + 1
void ZonalWeights(const float *directions, int dirCount, float *pZHW);

/// pZHW : output array of weights (must be size order)
void ZonalNormalization(int order, float *pZHN);

/// Expands a Spherical Harmonics vector into a Zonal Harmonics matrix transform.
/// Each band of spherical harmonics is decomposed into a set of Zonal Hamonics
/// with specific directions `directions`. (from Belcour/Nowrouzezahrai)
/// directions : input array of vec3, of size dirCount
/// pZHE : output array of the expansion, of size order^4, order = (dirCount-1) / 2 + 1
void ZonalExpansion(const float *directions, int dirCount, float *pZHE);

/// returns the inverse matrix of ZonalExpansion. Optimized since it's a block-diagonal matrix
void InverseZonalExpansion(const float *directions, int dirCount, float *pZHE);


/// Compute the product of the Zonal Weights and the InverseZonalExpansion matrix
/// AP is of the same size as the ZHW matrix : dirCount*(order^3), with order = (dirCount-1) / 2 + 1
void ComputeAPMatrix(const float *directions, int dirCount, float *AP);

/// Helper function for A*B = C matrix product, with Acols == Brows
/// TODO : Might push that in linmath.h instead
void MatrixProduct(const float *A, const float *B, float *C, int arows, int acols, int bcols);