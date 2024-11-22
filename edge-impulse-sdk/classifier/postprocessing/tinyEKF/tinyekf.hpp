/*
 * Extended Kalman Filter for embedded processors
 *
 * Copyright (C) 2024 Simon D. Levy
 *
 * MIT License
 */

#pragma once

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

template <typename T>
void print_arr(T *arr, int m, int n, const char *name = "arr") {
// verbose debug
#if EI_LOG_LEVEL == 5
    ei_printf("%s:\n", name);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            ei_printf("[%d][%d] = %g ", i, j, arr[i * n + j]);
        }
        ei_printf("\n");
    }
    ei_printf("\n");
#endif
}

class TinyEKF {
public:
    TinyEKF(const float* x0, uint32_t EKF_N, uint32_t EKF_M,
            float dt = 0.1,
            float *u = nullptr,
            float process_noise_scale = 0.1,
            float observation_noise_scale=0.1)
{
        // set private variables
        this->EKF_N = EKF_N;
        this->EKF_M = EKF_M;
        this->dt = dt;

        x = new float[this->EKF_N];
        memset(x, 0, sizeof(float) * this->EKF_N);
        // x is the state
        x[0] = x0[0];
        x[1] = x0[1];
        x[2] = x0[0];
        x[3] = x0[1];

        // print init x
        print_arr(x, 1, this->EKF_N, "init x");

        // F is the state transition model
        // self.F = np.array(
        //     [[1, 0, self.dt, 0],
        //      [0, 1, 0, self.dt],
        //      [0, 0, 1, 0],
        //      [0, 0, 0, 1]]
        // )

        F = new float[16];
        memset(F, 0, sizeof(float) * 16);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                F[i * 4 + j] = (i == j) ? 1 : 0;
            }
        }
        F[2] = F[7] = this->dt;

        // print F
        print_arr(F, 4, 4, "init F");

        // H is the observation model
        H = new float[8];
        memset(H, 0, sizeof(float) * 8);

        H[0] = H[5] = 1;

        // print H
        print_arr(H, 2, 4, "init H");

        // Q is the covariance of the process noise
        Q = new float[16];
        memset(Q, 0, sizeof(float) * 16);

        // self.Q = (
        //     np.array(
        //         [
        //             [(self.dt**4) / 4, 0, (self.dt**3) / 2, 0],
        //             [0, (self.dt**4) / 4, 0, (self.dt**3) / 2],
        //             [(self.dt**3) / 2, 0, self.dt**2, 0],
        //             [0, (self.dt**3) / 2, 0, self.dt**2],
        //         ]
        //     )
        //     * process_noise_scale**2
        // )

        Q[0] = Q[5] = pow(dt, 4) / 4;
        Q[2] = Q[7] = Q[8] = Q[13] = pow(dt, 3) / 2;
        Q[10] = Q[15] = pow(dt, 2);

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                Q[i * 4 + j] = Q[i * 4 + j] * pow(process_noise_scale, 2);
            }
        }

        // print Q
        print_arr(Q, 4, 4, "init Q");

        // R is the covariance of the observation noise
        R = new float[4];
        memset(R, 0, sizeof(float) * 4);

        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                R[i * 2 + j] = (i == j) ? (pow(observation_noise_scale, 2)) : 0;
            }
        }
        // print R
        print_arr(R, 2, 2, "init R");

        // control-input mode
        // self.B = np.array(
        //     [[(self.dt**2) / 2, 0],
        //      [0, (self.dt**2) / 2],
        //      [self.dt, 0],
        //      [0, self.dt]]
        // )

        B = new float[this->EKF_N * this->EKF_N * 2];
        memset(B, 0, sizeof(float) * this->EKF_N * this->EKF_N * 2);
        B[0] = B[3] = (dt * dt) / 2;
        B[4] = B[7] = dt;

        if (u == nullptr) {
            u = new float[2];
            u[0] = u[1] = 0.1;
        }
        this->u = u;

        // P is the predict / update transition
        P = new float[16];
        memset(P, 0, sizeof(float) * 16);

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                P[i * 4 + j] = (i == j) ? 1 : 0;
            }
        }

        // print P
        print_arr(P, 4, 4, "init P");
    }

    ~TinyEKF() {
        delete[] x;
        delete[] P;
        delete[] Q;
        delete[] F;
        delete[] H;
        delete[] R;
        delete[] B;
        delete[] u;
    }

    void predict(const float *fx);
    bool update(const float *z, const float *hx);
    float *x;
private:
    uint32_t EKF_N;
    uint32_t EKF_M;

    float *P;
    float *Q;
    float *F;
    float *H;
    float *R;

    float *B;
    float *u;
    float dt;

    void update_step3(float *GH);

    /// @private
    static void _mulmat(
            const float * a,
            const float * b,
            float * c,
            const int arows,
            const int acols,
            const int bcols)
    {
        for (int i=0; i<arows; ++i) {
            for (int j=0; j<bcols; ++j) {
                c[i*bcols+j] = 0;
                for (int k=0; k<acols; ++k) {
                    c[i*bcols+j] += a[i*acols+k] * b[k*bcols+j];
                }
            }
        }
    }

    /// @private
    static void _mulvec(
            const float * a,
            const float * x,
            float * y,
            const int m,
            const int n)
    {
        for (int i=0; i<m; ++i) {
            y[i] = 0;
            for (int j=0; j<n; ++j)
                y[i] += x[j] * a[i*n+j];
        }
    }

    /// @private
    static void _transpose(
            const float * a, float * at, const int m, const int n)
    {
        for (int i=0; i<m; ++i)
            for (int j=0; j<n; ++j) {
                at[j*m+i] = a[i*n+j];
            }
    }

    /// @private
    static void _addmat(
            const float * a, const float * b, float * c,
            const int m, const int n)
    {
        for (int i=0; i<m; ++i) {
            for (int j=0; j<n; ++j) {
                c[i*n+j] = a[i*n+j] + b[i*n+j];
            }
        }
    }

    /// @private
    static void _negate(float * a, const int m, const int n)
    {
        for (int i=0; i<m; ++i) {
            for (int j=0; j<n; ++j) {
                a[i*n+j] = -a[i*n+j];
            }
        }
    }

    /// @private
    static void _addeye(float * a, const int n)
    {
        for (int i=0; i<n; ++i) {
            a[i*n+i] += 1;
        }
    }

    /* Cholesky-decomposition matrix-inversion code, adapated from
    http://jean-pierre.moreau.pagesperso-orange.fr/Cplus/_choles_cpp.txt */

    /// @private
    static int _choldc1(float * a, float * p, const int n)
    {
        for (int i = 0; i < n; i++) {
            for (int j = i; j < n; j++) {
                float sum = a[i*n+j];
                for (int k = i - 1; k >= 0; k--) {
                    sum -= a[i*n+k] * a[j*n+k];
                }
                if (i == j) {
                    if (sum <= 0) {
                        return 1; /* error */
                    }
                    p[i] = sqrt(sum);
                }
                else {
                    a[j*n+i] = sum / p[i];
                }
            }
        }

        return 0; // success:w
    }

    /// @private
    static int _choldcsl(const float * A, float * a, float * p, const int n)
    {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                a[i*n+j] = A[i*n+j];
            }
        }
        if (_choldc1(a, p, n)) {
            return 1;
        }
        for (int i = 0; i < n; i++) {
            a[i*n+i] = 1 / p[i];
            for (int j = i + 1; j < n; j++) {
                float sum = 0;
                for (int k = i; k < j; k++) {
                    sum -= a[j*n+k] * a[k*n+i];
                }
                a[j*n+i] = sum / p[j];
            }
        }

        return 0; // success
    }

    /// @private
    static int _cholsl(const float * A, float * a, float * p, const int n)
    {
        if (_choldcsl(A,a,p,n)) {
            return 1;
        }

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                a[i*n+j] = 0.0;
            }
        }
        for (int i = 0; i < n; i++) {
            a[i*n+i] *= a[i*n+i];
            for (int k = i + 1; k < n; k++) {
                a[i*n+i] += a[k*n+i] * a[k*n+i];
            }
            for (int j = i + 1; j < n; j++) {
                for (int k = j; k < n; k++) {
                    a[i*n+j] += a[k*n+i] * a[k*n+j];
                }
            }
        }
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < i; j++) {
                a[i*n+j] = a[j*n+i];
            }
        }

        return 0; // success
    }

    /// @private
    static void _addvec(
            const float * a, const float * b, float * c, const int n)
    {
        for (int j=0; j<n; ++j) {
            c[j] = a[j] + b[j];
        }
    }

    /// @private
    static void _sub(
            const float * a, const float * b, float * c, const int n)
    {
        for (int j=0; j<n; ++j) {
            c[j] = a[j] - b[j];
        }
    }

    /// @private
    static bool invert(const float * a, float * ainv, uint32_t EKF_M)
    {
        float tmp[EKF_M];

        return _cholsl(a, ainv, tmp, EKF_M) == 0;
    }
};

void TinyEKF::predict(const float *fx) {

    // self.x = self.F @ self.x + self.B @ self.u

    float Bu[4];
    _mulmat(this->B, this->u, Bu, 4, 2, 1);

    // print Bu
    print_arr(Bu, 4, 1, "Bu");

    // print x before
    print_arr(this->x, 1, 4, "x before");

    float Fx[8];
    _mulmat(this->F, this->x, Fx, 4, 4, 2);

    // print Fx
    print_arr(Fx, 4, 2, "Fx");
    // print x after
    print_arr(this->x, 1, 4, "x after");

    //_addmat(Fx, Bu, this->x, 4, 1);
    this->x[0] = Fx[0] + Bu[0];
    this->x[1] = Fx[1] + Bu[1];
    this->x[2] = Fx[2] + Bu[0];
    this->x[3] = Fx[3] + Bu[1];
    this->x[4] = Fx[4] + Bu[2];
    this->x[5] = Fx[5] + Bu[3];
    this->x[6] = Fx[6] + Bu[2];
    this->x[7] = Fx[7] + Bu[3];

    // this is the formula for the next part
    // self.P_pre = np.dot(F, self.P_post).dot(F.T) + Q

    // np.dot(F, self.P_post)
    float FP[16];
    _mulmat(F, P, FP, 4, 4, 4);
    // print FP
    print_arr(FP, 4, 4, "FP");

    // F.T
    float Ft[16];
    _transpose(F, Ft, 4, 4);

    // .dot(F.T)
    float FPFt[16];
    _mulmat(FP, Ft, FPFt, 4, 4, 4);

    // + Q
    _addmat(FPFt, Q, P, 4, 4);

    // print P
    print_arr(P, 4, 4, "P");
}

bool TinyEKF::update(const float *z, const float *hx) {

    float Ht[8];
    _transpose(H, Ht, 2, 4);

    // print Ht
    print_arr(Ht, 4, 2, "Ht");

    float PHt[8];
    _mulmat(P, Ht, PHt, 4, 4, 2);

    float HP[8];
    _mulmat(H, P, HP, 2, 4, 4);

    float HpHt[4];
    _mulmat(HP, Ht, HpHt, 2, 4, 2);

    float HpHtR[4];
    _addmat(HpHt, R, HpHtR, 2, 2);

    float HPHtRinv[4];
    if (!invert(HpHtR, HPHtRinv, 2)) {
        return false;
    }

    float G[8];
    _mulmat(PHt, HPHtRinv, G, 4, 2, 2);

    // print G
    print_arr(G, 4, 2, "G");

    // print x
    print_arr(this->x, 1, 4, "x in update");

    // we get hx as an argument to function
    float z_hx[4];
    //_sub(z, Hx, z_hx, 2);
    z_hx[0] = z[0] - hx[0];
    z_hx[1] = z[1] - hx[1];
    z_hx[2] = z[0] - hx[0];
    z_hx[3] = z[1] - hx[1];

    // print z_hx
    print_arr(z_hx, 2, 2, "z_hx");

    float Gz_hx[8];
    _mulmat(G, z_hx, Gz_hx, 4, 2, 2);

    // // print Gz_hx
    print_arr(Gz_hx, 4, 2, "Gz_hx");

    _addvec(this->x, Gz_hx, this->x, 8);

    float GH[16];
    _mulmat(G, H, GH, 4, 2, 4);
    update_step3(GH);
    return true;
}

/// @private
void TinyEKF::update_step3(float *GH)
{
    _negate(GH, 4, 4);
    _addeye(GH, 4);

    float GHP[16];
    _mulmat(GH, P, GHP, 4, 4, 4);
    memcpy(P, GHP, 16 * sizeof(float));
}