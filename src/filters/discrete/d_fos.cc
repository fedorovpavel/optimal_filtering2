#include "d_fos.h"
#include "src/math/statistic.h"


namespace Filters
{

namespace Discrete
{

using Math::LinAlg::Pinv;
using Math::Statistic::Mean;
using Math::Statistic::Var;
using Math::Statistic::Cov;
using Math::MakeBlockVector;
using Math::MakeBlockMatrix;


FOS::FOS(Core::PtrFilterParameters params, Core::PtrTask task)
    : DiscreteFilter(params, task)
{
    m_info->setName(m_task->info()->type() + "ФМПд (p=" + std::to_string(task->dimX()) + ")");
}

void FOS::algorithm()
{
    Vector h, lambda;
    Matrix G, F, Psi, T;

    Array<Vector> sampleU(m_params->sampleSize());

    m_task->setStep(m_params->measurementStep());

    computeParams(0, sampleU, T);

    // Индекс k соответствует моменту времени tk = t0 + k * delta_t  (delta_t - интервал между измерениями):
    for (size_t k = 1; k < m_result.size(); ++k) {
        // Индекс s пробегает по всем элементам выборки:
        for (size_t s = 0; s < m_params->sampleSize(); ++s) {
            m_task->setTime(m_result[k - 1].time);
            // X_k = a(X_{k-1}); время t = t_{k-1}
            m_sampleX[s] = m_task->a(m_sampleX[s]);
            // вычисляем lambda, Psi;  время t = t_{k-1}:
            lambda = m_task->tau(sampleU[s], T);
            Psi    = m_task->Theta(sampleU[s], T);

            //ставим время обратно и продолжаем:
            m_task->setTime(m_result[k].time);

            h = m_task->h(lambda, Psi);
            G = m_task->G(lambda, Psi);
            F = m_task->F(lambda, Psi);

            m_sampleY[s] = m_task->b(m_sampleX[s]);
            m_sampleZ[s] = lambda + Psi * G.transpose() * Pinv(F) * (m_sampleY[s] - h);
        }
        writeResult(k);
        computeParams(k, sampleU, T);
    }
}

void FOS::computeParams(size_t k, Array<Vector> &u, Matrix &T)
{
    Vector        my, chi;
    Matrix        Gamma, GammaY, GammaZ, DxyDxz, Ddelta, Dxy, Dxz;
    Array<Vector> sampleDelta(m_params->sampleSize());

    // Индекс s пробегает по всем элементам выборки:
    for (size_t s = 0; s < m_params->sampleSize(); ++s) {
        MakeBlockVector(m_sampleY[s], m_sampleZ[s], sampleDelta[s]);
    }

    Ddelta = Var(sampleDelta);
    my     = Mean(m_sampleY);
    Dxy    = Cov(m_sampleX, m_sampleY);
    Dxz    = Cov(m_sampleX, m_sampleZ);
    MakeBlockMatrix(Dxy, Dxz, DxyDxz);

    Gamma  = DxyDxz * Pinv(Ddelta);
    GammaY = Gamma.leftCols(m_task->dimY());
    GammaZ = Gamma.rightCols(m_task->dimX()); // dimZ = dimX

    chi = m_result[k].meanX - GammaY * my - GammaZ * m_result[k].meanZ;
    T   = m_result[k].varX - GammaY * Dxy.transpose() - GammaZ * Dxz.transpose();

    // Индекс s пробегает по всем элементам выборки:
    for (size_t s = 0; s < m_params->sampleSize(); ++s) {
        u[s] = GammaY * m_sampleY[s] + GammaZ * m_sampleZ[s] + chi;
    }
}


} // end Discrete

} // end Filters
