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