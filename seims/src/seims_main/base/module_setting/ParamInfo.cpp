#include "ParamInfo.h"

using namespace std;

ParamInfo::ParamInfo(void) {
    Reset();
}

ParamInfo::~ParamInfo(void) {
}

float ParamInfo::GetAdjustedValue() {
    float res = Value;
    if (FloatEqual(res, NODATA_VALUE)) {  /// Do not change NoData value
        return res;
    }
    if (StringMatch(Use, PARAM_USE_Y)) {
        if (StringMatch(Change, PARAM_CHANGE_RC)) {
            res = Value * Impact;
        } else if (StringMatch(Change, PARAM_CHANGE_AC)) {
            res = Value + Impact;
        } else if (Change == PARAM_CHANGE_NC) {
            //don't change
        }
    }
    return res;
}

void ParamInfo::Adjust1DArray(int n, float *data) {
    if (StringMatch(Use, string(PARAM_USE_Y))) {
        if (StringMatch(Change, PARAM_CHANGE_RC) && !FloatEqual(Impact, 1.f)) {
#pragma omp parallel for
            for (int i = 0; i < n; i++) {
                if (!FloatEqual(data[i], NODATA_VALUE)) {  /// Do not change NoData value
                    data[i] *= Impact;
                }
            }
        } else if (StringMatch(Change, PARAM_CHANGE_AC) && !FloatEqual(Impact, 0.f)) {
#pragma omp parallel for
            for (int i = 0; i < n; i++) {
                if (!FloatEqual(data[i], NODATA_VALUE)) {  /// Do not change NoData value
                    data[i] += Impact;
                }
            }
        }
    }
}

void ParamInfo::Adjust1DRaster(int n, float *data) {
    Adjust1DArray(n, data);
}

void ParamInfo::Adjust2DArray(int n, float **data) {
#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        int curCols = (int) data[i][0];
        Adjust1DArray(curCols, data[i] + 1);
    }
}

void ParamInfo::Adjust2DRaster(int n, int lyrs, float **data) {
#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        Adjust1DArray(lyrs, data[i]);
    }
}

void ParamInfo::Reset(void) {
    Change = "";
    Description = "";
    Dimension = DT_Unknown;
    Impact = 0.f;
    Maximum = 0.f;
    Minimun = 0.f;
    ModuleID = "";
    Name = "";
    Source = "";
    Units = "";
    Use = "";
    Value = 0.f;
    DependPara = NULL;
    IsConstant = false;
    ClimateType = "";
    IsOutput = false;
    OutputToOthers = false;
    initialized = false;
}