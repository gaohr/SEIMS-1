/*!
 * \brief Add nitrate from rainfall to the soil profile as in SWAT rev. 637, nrain.f
 * \author Huiran Gao
 * \date May 2016
 *
 * \revised Liang-Jun Zhu
 * \date 2016-7-24
 * \note: 1. Delete m_cellWidth, m_nSoilLayers, m_sol_z, which are useless
 *        2. Change m_wshd_rno3 to store nitrate from rainfall of current day
 *        3. Remove output of m_sol_no3, which is redundant and unnecessary
 */
#ifndef SEIMS_MODULE_ATMDEP_H
#define SEIMS_MODULE_ATMDEP_H

#include "SimulationModule.h"

/** \defgroup ATMDEP
 * \ingroup Nutrient
 * \brief Calculate the atmospheric deposition of nitrogen, include nitrate and ammonia.
 */
/*!
 * \class AtmosphericDeposition
 * \ingroup ATMDEP
 */
class AtmosphericDeposition : public SimulationModule {
public:
    AtmosphericDeposition();

    ~AtmosphericDeposition();

    int Execute() OVERRIDE;

    void SetValue(const char *key, float data) OVERRIDE;

    void Set1DData(const char *key, int n, float *data) OVERRIDE;

    void Set2DData(const char *key, int nrows, int ncols, float **data) OVERRIDE;

    void GetValue(const char *key, float *value) OVERRIDE;

    bool CheckInputSize(const char *key, int n);

    bool CheckInputData();

    void  InitialOutputs();

private:
    /// size of array
    int m_nCells;
    ///// cell width of grid map (m)
    //float m_cellWidth;
    ///// soil layers
    //float *m_nSoilLayers;

    /// maximum soil layers
    int m_nMaxSoiLayers;

    /// parameters

    /// concentration of nitrate in the rain (mg N/L)
    float m_rcn;
    /// concentration of ammonia in the rain (mg N/L)
    float m_rca;
    ///atmospheric dry deposition of nitrates (kg/ha)
    float m_drydep_no3;
    ///atmospheric dry deposition of ammonia (kg/ha)
    float m_drydep_nh4;

    /// inputs

    /// precipitation (mm H2O)
    float *m_preci;
    ///// root depth from the soil surface
    //float **m_sol_z;

    ///amount of ammonium in layer (kg/ha)
    float **m_sol_nh4;
    /// amount of nitrate in layer (kg/ha)
    float **m_sol_no3;

    /// temporaries

    /// nitrate added by rainfall (kg/ha)
    float m_addrnh4;
    /// ammonium added by rainfall (kg/ha)
    float m_addrno3;

    /// outputs

    /// amount of NO3 added to soil by rainfall in watershed on current day (kg/ha)
    float m_wshd_rno3;
};
#endif /* SEIMS_MODULE_ATMDEP_H */
