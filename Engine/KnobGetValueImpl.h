/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

#ifndef KNOBGETVALUEIMPL_H
#define KNOBGETVALUEIMPL_H

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "Global/Macros.h"

#include "KnobPrivate.h"

#include "Engine/EffectInstance.h"
#include "Engine/KnobItemsTable.h"
#include "Engine/RenderValuesCache.h"
#include "Engine/TreeRenderNodeArgs.h"

#include "Engine/EngineFwd.h"

NATRON_NAMESPACE_ENTER

template <typename T>
getValueFromCachedExpressionResult(const KnobExpressionResultPtr& cachedValue, T* result)
{
    double value;
    cachedValue->getResult(&value, 0);
    *result = (T)value;
}

template <>
getValueFromCachedExpressionResult(const KnobExpressionResultPtr& cachedValue, std::string* result)
{
    cachedValue->getResult(0, result);
}

template <typename T>
setValueFromCachedExpressionResult(const KnobExpressionResultPtr& cachedValue, const T& result)
{
    double value = (double)result;
    cachedValue->setResult(value, std::string());
}

template <>
setValueFromCachedExpressionResult(const KnobExpressionResultPtr& cachedValue, const std::string& result)
{
    cachedValue->setResult(0, result);
}

template <typename T>
bool
Knob<T>::getValueFromExpression(TimeValue time,
                                ViewIdx view,
                                DimIdx dimension,
                                bool clamp,
                                T* ret)
{
    ///Prevent recursive call of the expression
    if (getExpressionRecursionLevel() > 0) {
        return false;
    }

    if (dimension < 0 || dimension >= getNDimensions()) {
        throw std::invalid_argument("Knob::getValueFromExpression: dimension out of range");
    }

    ViewIdx view_i = getViewIdxFromGetSpec(view);

    // Check for a cached expression result


    KnobHolderPtr holder = getHolder();
    EffectInstancePtr effect = toEffectInstance(holder);
    KnobTableItemPtr tableItem = toKnobTableItem(holder);
    if (tableItem) {
        effect = tableItem->getModel()->getNode()->getEffectInstance();
    }
    U64 effectHash = 0;
    assert(effect);
    if (effect) {
        TreeRenderNodeArgsPtr render = effect->getCurrentRender_TLS();
        if (render) {
            render->getTimeViewInvariantHash(&effectHash);
        }
        if (effectHash == 0) {
            ComputeHashArgs hashArgs;
            hashArgs.render = render;
            hashArgs.time = time;
            hashArgs.view = view;
            hashArgs.hashType = HashableObject::eComputeHashTypeTimeViewInvariant;
            effectHash = effect->computeHash(hashArgs);
        }
    }


    KnobExpressionKey cacheKey(effectHash, dimension, time, view, getName());

    CacheEntryLockerPtr cacheAccess = appPTR->getCache()->get(cacheKey);

    CacheEntryLocker::CacheEntryStatusEnum cacheStatus = cacheAccess->getStatus();
    while (cacheStatus == CacheEntryLocker::eCacheEntryStatusComputationPending) {
        cacheStatus = cacheAccess->waitForPendingEntry();
    }

    KnobExpressionResultPtr cachedResult;
    if (cacheStatus == CacheEntryLocker::eCacheEntryStatusCached) {
        cachedResult = boost::dynamic_pointer_cast<KnobExpressionResult>(cacheAccess->getCachedEntry());
        if (cachedResult) {
            getValueFromCachedExpressionResult(cachedResult, ret);
            return true;
        }
    }
    assert(cacheStatus == CacheEntryLocker::eCacheEntryStatusMustCompute);

    cachedResult = KnobExpressionResult::create(cacheKey);

    bool exprWasValid = isExpressionValid(dimension, view_i, 0);
    {
        EXPR_RECURSION_LEVEL();
        std::string error;
        bool exprOk = evaluateExpression(time, view_i,  dimension, ret, &error);
        if (!exprOk) {
            setExpressionInvalid(dimension, view_i, false, error);

            return false;
        } else {
            if (!exprWasValid) {
                setExpressionInvalid(dimension, view_i, true, error);
            }
        }
    }

    if (clamp) {
        *ret =  clampToMinMax(*ret, dimension);
    }

    setValueFromCachedExpressionResult(cachedResult, *ret);
    cacheAccess.setEntry(cachedResult);

    return true;
} // getValueFromExpression

template <>
bool
KnobStringBase::getValueFromExpression_pod(TimeValue time,
                                           ViewIdx view,
                                           DimIdx dimension,
                                           bool /*clamp*/,
                                           double* ret)
{
    ///Prevent recursive call of the expression


    if (getExpressionRecursionLevel() > 0) {
        return false;
    }

    if (dimension < 0 || dimension >= getNDimensions()) {
        throw std::invalid_argument("Knob::getValueFromExpression_pod: dimension out of range");
    }

    ViewIdx view_i = getViewIdxFromGetSpec(view);

    bool exprWasValid = isExpressionValid(dimension, view_i, 0);
    {
        EXPR_RECURSION_LEVEL();
        std::string error;
        bool exprOk = evaluateExpression_pod(time, view_i,  dimension, ret, &error);
        if (!exprOk) {
            setExpressionInvalid(dimension, ViewSetSpec(view_i), false, error);

            return false;
        } else {
            if (!exprWasValid) {
                setExpressionInvalid(dimension, ViewSetSpec(view_i), true, error);
            }
        }
    }

    return true;
} // getValueFromExpression_pod

template <typename T>
bool
Knob<T>::getValueFromExpression_pod(TimeValue time,
                                    ViewIdx view,
                                    DimIdx dimension,
                                    bool clamp,
                                    double* ret)
{
    ///Prevent recursive call of the expression


    if (getExpressionRecursionLevel() > 0) {
        return false;
    }

    if (dimension < 0 || dimension >= getNDimensions()) {
        throw std::invalid_argument("Knob::getValueFromExpression_pod: dimension out of range");
    }

    ViewIdx view_i = getViewIdxFromGetSpec(view);
    

    ///Check first if a value was already computed:


    {
        QMutexLocker k(&_defaultValueMutex);
        typename PerViewFrameValueMap::iterator foundView = _exprRes[dimension].find(view_i);
        if (foundView != _exprRes[dimension].end()) {
            typename FrameValueMap::iterator foundValue = foundView->second.find(time);
            if ( foundValue != foundView->second.end() ) {
                *ret = foundValue->second;

                return true;
            }
        }
        
    }



    bool exprWasValid = isExpressionValid(dimension, view_i, 0);
    {
        EXPR_RECURSION_LEVEL();
        std::string error;
        bool exprOk = evaluateExpression_pod(time, view_i, dimension, ret, &error);
        if (!exprOk) {
            setExpressionInvalid(dimension, ViewSetSpec(view_i), false, error);

            return false;
        } else {
            if (!exprWasValid) {
                setExpressionInvalid(dimension, ViewSetSpec(view_i), true, error);
            }
        }
    }

    if (clamp) {
        *ret =  clampToMinMax(*ret, dimension);
    }
    
    //QWriteLocker k(&_valueMutex);
    _exprRes[dimension][view_i][time] = *ret;
    
    return true;
} // getValueFromExpression_pod

template <typename T>
T
Knob<T>::getValueInternal(const boost::shared_ptr<Knob<T> >& thisShared,
                          const RenderValuesCachePtr& valuesCache,
                          double tlsCurrentTime,
                          DimIdx dimension,
                          ViewIdx view,
                          bool clamp)
{
    ValueKnobDimView<T>* dataForDimView = dynamic_cast<ValueKnobDimView<T>*>(getDataForDimView(dimension, view).get());
    if (!dataForDimView) {
        return T();
    }
    {
        QMutexLocker k(&dataForDimView->valueMutex);
        if (clamp) {
            T ret = clampToMinMax(dataForDimView->value, dimension);
            if (valuesCache) {
                valuesCache->setCachedKnobValue<T>(thisShared, tlsCurrentTime, dimension, view, ret);
            }
            return ret;
        } else {
            const T& ret = dataForDimView->value;
            if (valuesCache) {
                valuesCache->setCachedKnobValue<T>(thisShared, tlsCurrentTime, dimension, view, ret);
            }
            return ret;
        }
    }
} // getValueInternal

template <typename T>
T
Knob<T>::getValue(DimIdx dimension,
                  ViewIdx view,
                  bool clamp)
{

    if ( ( dimension >= getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getValue: dimension out of range");
    }



    // Figure out the view to read
    ViewIdx view_i = getViewIdxFromGetSpec(view);

    boost::shared_ptr<Knob<T> > thisShared = boost::dynamic_pointer_cast<Knob<T> >(shared_from_this());


    double tlsCurrentTime = 0;
    RenderValuesCachePtr valuesCache = getHolderRenderValuesCache(&tlsCurrentTime);
    // Check if value is already in TLS when rendering
    if (valuesCache) {
        T ret;
        if (valuesCache->getCachedKnobValue<T>(thisShared, tlsCurrentTime, dimension, view_i, &ret)) {
            return ret;
        }
    }


    // If an expression is set, read from expression
    std::string hasExpr = getExpression(dimension, view_i);
    if ( !hasExpr.empty() ) {
        T ret;
        TimeValue time = valuesCache ? tlsCurrentTime : getCurrentTime_TLS();
        if ( getValueFromExpression(time, view, dimension, clamp, &ret) ) {
            if (valuesCache) {
                valuesCache->setCachedKnobValue<T>(thisShared, tlsCurrentTime, dimension, view_i, ret);
            }
            return ret;
        }
    }

    // If animated, call getValueAtTime instead
    if ( isAnimated(dimension, view) ) {
        TimeValue time = valuesCache ? tlsCurrentTime : getCurrentTime_TLS();
        return getValueAtTime(time, dimension, view, clamp);
    }

    return getValueInternal(thisShared, valuesCache, tlsCurrentTime, dimension, view_i, clamp);

} // getValue


template <typename T>
bool
Knob<T>::getValueFromCurve(TimeValue time,
                           ViewIdx view,
                           DimIdx dimension,
                           bool clamp,
                           T* ret)
{

    ViewIdx view_i = getViewIdxFromGetSpec(view);
    CurvePtr curve = getAnimationCurve(view_i, dimension);

    if ( curve && (curve->getKeyFramesCount() > 0) ) {
        //getValueAt already clamps to the range for us
        *ret = (T)curve->getValueAt(time, clamp);

        return true;
    }

    return false;
} // getValueFromCurve

template <>
bool
KnobStringBase::getValueFromCurve(TimeValue time,
                                  ViewIdx view,
                                  DimIdx dimension,
                                  bool /*clamp*/,
                                  std::string* ret)
{
    AnimatingKnobStringHelper* isStringAnimated = dynamic_cast<AnimatingKnobStringHelper* >(this);

    if (isStringAnimated) {
        *ret = isStringAnimated->getStringAtTime(time, view);
        ///ret is not empty if the animated string knob has a custom interpolation
        if ( !ret->empty() ) {
            return true;
        }
    }
    assert( ret->empty() );
    ViewIdx view_i = getViewIdxFromGetSpec(view);
    CurvePtr curve = getAnimationCurve(view_i, dimension);
    if ( curve && (curve->getKeyFramesCount() > 0) ) {
        assert(isStringAnimated);
        if (isStringAnimated) {
            isStringAnimated->stringFromInterpolatedValue(curve->getValueAt(time), view_i, ret);
        }

        return true;
    }

    return false;
} // getValueFromCurve


template <>
double
KnobStringBase::getRawCurveValueAt(TimeValue time,
                                   ViewIdx view,
                                   DimIdx dimension)
{
    ViewIdx view_i = getViewIdxFromGetSpec(view);
    CurvePtr curve  = getAnimationCurve(view_i, dimension);

    if ( curve && (curve->getKeyFramesCount() > 0) ) {
        //getValueAt already clamps to the range for us
        return curve->getValueAt(time, false); //< no clamping to range!
    }

    return 0;
}

template <typename T>
double
Knob<T>::getRawCurveValueAt(TimeValue time,
                            ViewIdx view,
                            DimIdx dimension)
{
    CurvePtr curve  = getAnimationCurve(view, dimension);

    if ( curve && (curve->getKeyFramesCount() > 0) ) {
        //getValueAt already clamps to the range for us
        return curve->getValueAt(time, false); //< no clamping to range!
    }
    ViewIdx view_i = getViewIdxFromGetSpec(view);

    ValueKnobDimView<T>* dataForDimView = dynamic_cast<ValueKnobDimView<T>*>(getDataForDimView(dimension, view_i).get());
    if (!dataForDimView) {
        return T();
    }
    T ret;
    {
        QMutexLocker k(&dataForDimView->valueMutex);
        ret = dataForDimView->value;
    }
    return clampToMinMax(ret, dimension);
} // getRawCurveValueAt

template <typename T>
double
Knob<T>::getValueAtWithExpression(TimeValue time,
                                  ViewIdx view,
                                  DimIdx dimension)
{
    bool exprValid = isExpressionValid(dimension, view, 0);
    std::string expr = getExpression(dimension, view);

    if (!expr.empty() && exprValid) {
        double ret;
        if ( getValueFromExpression_pod(time, view, dimension, false, &ret) ) {
            return ret;
        }
    }

    return getRawCurveValueAt(time, view, dimension);
} // getValueAtWithExpression

template<typename T>
T
Knob<T>::getValueAtTime(TimeValue time,
                        DimIdx dimension,
                        ViewIdx view,
                        bool clamp)
{

    if  ( ( dimension >= getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getValueAtTime: dimension out of range");
    }


    // Figure out the view to read
    ViewIdx view_i = getViewIdxFromGetSpec(view);

    boost::shared_ptr<Knob<T> > thisShared = boost::dynamic_pointer_cast<Knob<T> >(shared_from_this());

    RenderValuesCachePtr valuesCache = getHolderRenderValuesCache();
    // Check if value is already in TLS when rendering
    if (valuesCache) {
        T ret;
        if (valuesCache->getCachedKnobValue<T>(thisShared, time, dimension, view_i, &ret)) {
            return ret;
        }
    }

    std::string hasExpr = getExpression(dimension);
    if ( !hasExpr.empty() ) {
        T ret;
        if ( getValueFromExpression(time, /*view*/ ViewIdx(0), dimension, clamp, &ret) ) {
            if (valuesCache) {
                valuesCache->setCachedKnobValue<T>(thisShared, time, dimension, view_i, ret);
            }
            return ret;
        }
    }



    T ret;
    if ( getValueFromCurve(time, view_i, dimension, clamp, &ret) ) {
        if (valuesCache) {
            valuesCache->setCachedKnobValue<T>(thisShared, time, dimension, view_i, ret);
        }

        return ret;
    }

    // If the knob as no keys at this dimension/view, return the underlying value
    return getValueInternal(thisShared, valuesCache, time, dimension, view_i, clamp);
} // getValueAtTime


template<typename T>
double
Knob<T>::getDerivativeAtTime(TimeValue time,
                             ViewIdx view,
                             DimIdx dimension)
{
    if ( ( dimension > getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getDerivativeAtTime(): Dimension out of range");
    }
    {
        std::string expr = getExpression(dimension);
        if ( !expr.empty() ) {
            // Compute derivative by finite differences, using values at t-0.5 and t+0.5
            return ( getValueAtTime(time + 0.5, dimension, view) - getValueAtTime(time - 0.5, dimension, view) ) / 2.;
        }
    }

    ViewIdx view_i = getViewIdxFromGetSpec(view);

    CurvePtr curve  = getAnimationCurve(view_i, dimension);
    if (curve->getKeyFramesCount() > 0) {
        return curve->getDerivativeAt(time);
    } else {
        /*if the knob as no keys at this dimension, the derivative is 0.*/
        return 0.;
    }
}

template<>
double
KnobStringBase::getDerivativeAtTime(TimeValue /*time*/,
                                    ViewIdx /*view*/,
                                    DimIdx /*dimension*/)
{
    throw std::invalid_argument("Knob<string>::getDerivativeAtTime() not available");
}

// Compute integral using Simpsons rule:
// \int_a^b f(x) dx = (b-a)/6 * (f(a) + 4f((a+b)/2) + f(b))
template<typename T>
double
Knob<T>::getIntegrateFromTimeToTimeSimpson(TimeValue time1,
                                           TimeValue time2,
                                           ViewIdx view,
                                           DimIdx dimension)
{
    double fa = getValueAtTime(time1, dimension, view);
    double fm = getValueAtTime( (time1 + time2) / 2, dimension, view );
    double fb = getValueAtTime(time2, dimension, view);

    return (time2 - time1) / 6 * (fa + 4 * fm + fb);
}

template<typename T>
double
Knob<T>::getIntegrateFromTimeToTime(TimeValue time1,
                                    TimeValue time2,
                                    ViewIdx view,
                                    DimIdx dimension)
{
    if ( ( dimension > getNDimensions() ) || (dimension < 0) ) {
        throw std::invalid_argument("Knob::getIntegrateFromTimeToTime(): Dimension out of range");
    }
    {
        std::string expr = getExpression(dimension);
        if ( !expr.empty() ) {
            // Compute integral using Simpsons rule:
            // \int_a^b f(x) dx = (b-a)/6 * (f(a) + 4f((a+b)/2) + f(b))
            // The interval from time1 to time2 is split into intervals where bounds are at integer values
            int i = std::ceil(time1);
            int j = std::floor(time2);
            if (i > j) { // no integer values in the interval
                return getIntegrateFromTimeToTimeSimpson(time1, time2, view, dimension);
            }
            double val = 0.;
            // start integrating over the interval
            // first chunk
            if (time1 < i) {
                val += getIntegrateFromTimeToTimeSimpson(time1, i, view, dimension);
            }
            // integer chunks
            for (int t = i; t < j; ++t) {
                val += getIntegrateFromTimeToTimeSimpson(t, t + 1, view, dimension);
            }
            // last chunk
            if (j < time2) {
                val += getIntegrateFromTimeToTimeSimpson(j, time2, view, dimension);
            }

            return val;
        }
    }

    ViewIdx view_i = getViewIdxFromGetSpec(view);

    CurvePtr curve  = getAnimationCurve(view_i, dimension);
    if (curve->getKeyFramesCount() > 0) {
        return curve->getIntegrateFromTo(time1, time2);
    } else {
        // if the knob as no keys at this dimension, the integral is trivial
        T value = getValue(dimension, view_i);
        return value * (time2 - time1);
    }
} // getIntegrateFromTimeToTime

template<>
double
KnobStringBase::getIntegrateFromTimeToTimeSimpson(double /*time1*/,
                                                  double /*time2*/,
                                                  ViewIdx /*view*/,
                                                  DimIdx /*dimension*/)
{
    return 0; // dummy value
}

template<>
double
KnobStringBase::getIntegrateFromTimeToTime(double /*time1*/,
                                           double /*time2*/,
                                           ViewIdx /*view*/,
                                           DimIdx /*dimension*/)
{
    throw std::invalid_argument("Knob<string>::getIntegrateFromTimeToTime() not available");
}



NATRON_NAMESPACE_EXIT

#endif // KNOBGETVALUEIMPL_H
