/**
 * ****************************************************************************
 * Copyright (c) 2015, Robert Lukierski.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * ****************************************************************************
 * Ideal Gayer-Baretto-Mei generic camera model.
 * ****************************************************************************
 */

#ifndef IDEAL_GENERIC_CAMERA_MODEL_HPP
#define IDEAL_GENERIC_CAMERA_MODEL_HPP

#include <CameraModelHelpers.hpp>

// fwd
namespace camera 
{
template<typename _Scalar, int _Options = 0> class IdealGenericCameraModel;

namespace internal
{
static constexpr unsigned int IdealGenericCameraModelParameterCount = 9;
}
} 

// Eigen Traits 
namespace Eigen 
{
    namespace internal 
    {
        template<typename _Scalar, int _Options>
        struct traits<camera::IdealGenericCameraModel<_Scalar,_Options> > 
        {
            typedef _Scalar Scalar; 
            typedef Matrix<Scalar,camera::internal::IdealGenericCameraModelParameterCount,1> ComplexType;
        };
        
        template<typename _Scalar, int _Options>
        struct traits<Map<camera::IdealGenericCameraModel<_Scalar>, _Options> > : traits<camera::IdealGenericCameraModel<_Scalar, _Options> > 
        {
            typedef _Scalar Scalar;
            typedef Map<Matrix<Scalar,camera::internal::IdealGenericCameraModelParameterCount,1>,_Options> ComplexType;
        };
        
        template<typename _Scalar, int _Options>
        struct traits<Map<const camera::IdealGenericCameraModel<_Scalar>, _Options> > : traits<const camera::IdealGenericCameraModel<_Scalar, _Options> > 
        {
            typedef _Scalar Scalar;
            typedef Map<const Matrix<Scalar,camera::internal::IdealGenericCameraModelParameterCount,1>,_Options> ComplexType;
        };    
    }
}

namespace camera
{
/**
 * Ideal Generic Camera Model, Model Specific Functions.
 * Also called Geyer Model.
 */
template<typename Derived>
class IdealGenericCameraModelBase : public CameraFunctions<Derived>, public ComplexTypes<typename Eigen::internal::traits<Derived>::Scalar>
{
    typedef CameraFunctions<Derived> FunctionsBase;
public:
    // various helpers and types
    typedef typename Eigen::internal::traits<Derived>::Scalar Scalar;
    typedef typename Eigen::internal::traits<Derived>::ComplexType& ComplexReference;
    typedef const typename Eigen::internal::traits<Derived>::ComplexType& ConstComplexReference;
    static constexpr CameraModelType ModelType = CameraModelType::IdealGeneric;
    
    using FunctionsBase::forward;
    using FunctionsBase::inverse;
    using FunctionsBase::inverseAtDistance;
    using FunctionsBase::twoFrameProject;
    using FunctionsBase::worldToCamera;
    using FunctionsBase::cameraToWorld;
    using FunctionsBase::pixelValid;
    using FunctionsBase::pixelValidSquare;
    using FunctionsBase::pixelValidCircular;
    using FunctionsBase::resizeViewport;
    
    template<typename NewScalarType>
    EIGEN_DEVICE_FUNC inline IdealGenericCameraModel<NewScalarType> cast() const { return IdealGenericCameraModel<NewScalarType>(access().template cast<NewScalarType>()); }
    
    template<typename OtherDerived> 
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE IdealGenericCameraModelBase<Derived>& operator=(const IdealGenericCameraModelBase<OtherDerived> & other) { access_nonconst() = other.access(); return *this; }
    
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ConstComplexReference access() const  { return static_cast<const Derived*>(this)->access(); }
private:
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ComplexReference access_nonconst()  { return static_cast<Derived*>(this)->access_nonconst(); }
public:
        
    static constexpr unsigned int NumParameters = camera::internal::IdealGenericCameraModelParameterCount;
    static constexpr unsigned int ParametersToOptimize = NumParameters - 4;
    static constexpr bool CalibrationSupported = true;
    
    template<typename T = Scalar>
    static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE typename ComplexTypes<T>::PointT inverse(const Derived& ccd, T x, T y) 
    {
        typename ComplexTypes<T>::PointT ret;
        
        // inverse intrinsics pixel -> image plane
        typename ComplexTypes<T>::PixelT pix_dist;
        pix_dist(0) = (x - ccd.u0()) / ccd.fx();
        pix_dist(1) = (y - ccd.v0()) / ccd.fy();
        
        // inverse perspective - pixel to point
        const T x2 = pix_dist(0) * pix_dist(0);
        const T y2 = pix_dist(1) * pix_dist(1);
        const T eps2 = T(ccd.epsilon()) * T(ccd.epsilon());
        
        //         eps + sqrt( 1 + (1-eps^2) * (x^2 + y^2) )
        // term = -----------------------------------------
        //                    x^2 + y^2 + 1
        const T term = (ccd.epsilon() + sqrt(T(1.0f) + (T(1.0f) - eps2) * (x2 + y2))) / (x2 + y2 + T(1.0f));
        
        ret(0) = term * pix_dist(0);
        ret(1) = term * pix_dist(1); 
        ret(2) = term - ccd.epsilon();
        
        return ret;
    }
    
    template<typename T = Scalar>
    static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE typename ComplexTypes<T>::PixelT forward(const Derived& ccd, const typename ComplexTypes<T>::PointT& tmp_pt) 
    {
        typename ComplexTypes<T>::PixelT ret, p;
        
        // unit vector
        const typename ComplexTypes<T>::PointT unit_pt = tmp_pt.normalized();        
        
        // perspective
        p = unit_pt.template topRows<2>() / (unit_pt(2) + ccd.epsilon());
        
        // intrinsics
        ret(0) = ccd.fx() * p(0) + ccd.u0();
        ret(1) = ccd.fy() * p(1) + ccd.v0();
        
        return ret;
    }
    
    template<typename T = Scalar>
    static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE bool pixelValidSquare(const Derived& ccd, T x, T y) 
    {
        if((x >= T(0.0)) && (x < ccd.width()) && (y >= T(0.0)) && (y < ccd.height()))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    
    template<typename T = Scalar>
    static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE bool pixelValidCircular(const Derived& ccd, T x, T y) 
    {
        if(ccd.r1() <= 0.0) // no r1
        {
            const T x2 = (x - ccd.u0()) * (x - ccd.u0());
            const T y2 = (y - ccd.v0()) * (y - ccd.v0());
            if(x2 + y2 < ccd.r2() * ccd.r2())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            const T x2 = (x - ccd.u0()) * (x - ccd.u0());
            const T y2 = (y - ccd.v0()) * (y - ccd.v0());
            const T r12 = ccd.r1() * ccd.r1();
            const T r22 = ccd.r2() * ccd.r2();
            
            if(((x2 + y2) > r12) && ((x2 + y2) < r22))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    
    template<typename T = Scalar>
    static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void resizeViewport(Derived& ccd, const T& new_width, const T& new_height)
    {
        const T x_ratio = new_width / ccd.width();
        const T y_ratio = new_height / ccd.height();
        const T r_ratio = std::min(x_ratio, y_ratio);
        
        ccd.fx(ccd.fx() * x_ratio);
        ccd.fy(ccd.fy() * y_ratio);
        ccd.u0(ccd.u0() * x_ratio);
        ccd.v0(ccd.v0() * y_ratio);
        ccd.r1(ccd.r1() * r_ratio);
        ccd.r2(ccd.r2() * r_ratio);
        ccd.width(new_width);
        ccd.height(new_height);
    }
        
    // access to camera model parameters
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& fx() const { return data()[0]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& fy() const { return data()[1]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& u0() const { return data()[2]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& v0() const { return data()[3]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& epsilon() const { return data()[4]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& width() const { return data()[5]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& height() const { return data()[6]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& r1() const { return data()[7]; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar& r2() const { return data()[8]; }
    
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void fx(const Scalar& v) { data()[0] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void fy(const Scalar& v) { data()[1] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void u0(const Scalar& v) { data()[2] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void v0(const Scalar& v) { data()[3] = v; } 
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void epsilon(const Scalar& v) { data()[4] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void width(const Scalar& v) { data()[5] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void height(const Scalar& v) { data()[6] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void r1(const Scalar& v) { data()[7] = v; }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void r2(const Scalar& v) { data()[8] = v; }
    
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Scalar* data() { return access_nonconst().data(); }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE const Scalar* data() const { return access().data(); }
    
#ifdef CAMERA_MODELS_HAVE_SERIALIZER
    template <typename Archive>
    void serialize(Archive & archive, std::uint32_t const version)
    { 
        CAMERA_MODELS_SERIALIZE(archive,"fx",data()[0]);
        CAMERA_MODELS_SERIALIZE(archive,"fy",data()[1]);
        CAMERA_MODELS_SERIALIZE(archive,"u0",data()[2]);
        CAMERA_MODELS_SERIALIZE(archive,"v0",data()[3]);
        CAMERA_MODELS_SERIALIZE(archive,"epsilon",data()[4]);
        CAMERA_MODELS_SERIALIZE(archive,"r1",data()[5]);
        CAMERA_MODELS_SERIALIZE(archive,"r2",data()[6]);
        CAMERA_MODELS_SERIALIZE(archive,"width",data()[7]);
        CAMERA_MODELS_SERIALIZE(archive,"height",data()[8]);
    }
#endif // CAMERA_MODELS_HAVE_SERIALIZER
};

/**
 * Ideal Generic Camera Model, Eigen storage.
 */
template<typename _Scalar, int _Options>
class IdealGenericCameraModel : public IdealGenericCameraModelBase<IdealGenericCameraModel<_Scalar, _Options>>
{
    typedef IdealGenericCameraModelBase<IdealGenericCameraModel<_Scalar, _Options>> Base;
public:
    typedef typename Eigen::internal::traits<IdealGenericCameraModel<_Scalar,_Options> >::Scalar Scalar;
    typedef typename Eigen::internal::traits<IdealGenericCameraModel<_Scalar,_Options> >::ComplexType& ComplexReference;
    typedef const typename Eigen::internal::traits<IdealGenericCameraModel<_Scalar,_Options> >::ComplexType& ConstComplexReference;
    
    template<class OtherT> using GetOtherType = IdealGenericCameraModel<OtherT, _Options>;
    
    static constexpr unsigned int NumParameters = Base::NumParameters;
    static constexpr unsigned int ParametersToOptimize = Base::ParametersToOptimize;
    static constexpr bool CalibrationSupported = Base::CalibrationSupported;
    
    friend class IdealGenericCameraModelBase<IdealGenericCameraModel<_Scalar, _Options>>;
    
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    EIGEN_DEVICE_FUNC inline IdealGenericCameraModel(Scalar fx, Scalar fy, Scalar u0, Scalar v0, Scalar epsilon = 0.0, Scalar w = 0.0 , Scalar h = 0.0, Scalar r1 = 0.0, Scalar r2 = 0.0)
    {
        access_nonconst() << fx , fy , u0 , v0 , epsilon, w , h , r1, r2;
    }
    
    EIGEN_DEVICE_FUNC inline IdealGenericCameraModel() : parameters(Eigen::Matrix<Scalar,NumParameters,1>::Zero()) { }
    EIGEN_DEVICE_FUNC inline IdealGenericCameraModel(const typename Eigen::internal::traits<IdealGenericCameraModel<_Scalar,_Options> >::ComplexType& vec) : parameters(vec) { }
    
    EIGEN_DEVICE_FUNC inline IdealGenericCameraModel& operator=(const typename Eigen::internal::traits<IdealGenericCameraModel<_Scalar,_Options> >::ComplexType& vec) { access_nonconst() = vec; return *this; }
    
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ConstComplexReference access() const { return parameters; }
protected:
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ComplexReference access_nonconst() { return parameters; }
    Eigen::Matrix<Scalar,NumParameters,1> parameters;
};

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const IdealGenericCameraModel<T>& p)
{
    os << "IdealGenericCameraModel(fx = " << p.fx() << ", fy = " << p.fy() << ", u0 = " << p.u0() << ", v0 = " << p.v0() << ", eps = " << p.epsilon() << "," << p.width() << " x " << p.height() << ", r1 = " << p.r1() << ", r2 = " << p.r2() << ")";
    return os;
}

}

namespace Eigen 
{
/**
 * Ideal Generic Camera Model, Eigen Map.
 */
template<typename _Scalar, int _Options>
class Map<camera::IdealGenericCameraModel<_Scalar>, _Options> : public camera::IdealGenericCameraModelBase<Map<camera::IdealGenericCameraModel<_Scalar>, _Options>>
{
    typedef camera::IdealGenericCameraModelBase<Map<camera::IdealGenericCameraModel<_Scalar>, _Options>> Base;
    
public:
    typedef typename internal::traits<Map>::Scalar Scalar;
    typedef typename internal::traits<Map>::ComplexType& ComplexReference;
    typedef const typename internal::traits<Map>::ComplexType& ConstComplexReference;
    
    static constexpr unsigned int NumParameters = Base::NumParameters;
    static constexpr unsigned int ParametersToOptimize = Base::ParametersToOptimize;
    static constexpr bool CalibrationSupported = Base::CalibrationSupported;
    
    friend class camera::IdealGenericCameraModelBase<Map<camera::IdealGenericCameraModel<_Scalar>, _Options>>;
    
    EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Map)
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Map(Scalar* coeffs) : parameters(coeffs)  { }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ConstComplexReference access() const { return parameters; }
protected:
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ComplexReference access_nonconst() { return parameters; }
    
    Map<Matrix<Scalar,NumParameters,1>,_Options> parameters;
};

/**
 * Ideal Generic Camera Model, Eigen Map const.
 */
template<typename _Scalar, int _Options>
class Map<const camera::IdealGenericCameraModel<_Scalar>, _Options> : public camera::IdealGenericCameraModelBase<Map<const camera::IdealGenericCameraModel<_Scalar>, _Options>>
{
    typedef camera::IdealGenericCameraModelBase<Map<const camera::IdealGenericCameraModel<_Scalar>, _Options>> Base;
public:
    typedef typename internal::traits<Map>::Scalar Scalar;
    typedef const typename internal::traits<Map>::ComplexType & ConstComplexReference;
    
    static constexpr unsigned int NumParameters = Base::NumParameters;
    static constexpr unsigned int ParametersToOptimize = Base::ParametersToOptimize;
    static constexpr bool CalibrationSupported = Base::CalibrationSupported;
    
    EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Map)
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Map(const Scalar* coeffs) : parameters(coeffs) { }
    EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ConstComplexReference access() const  { return parameters; }
protected:
    const Map<const Matrix<Scalar,NumParameters,1>,_Options> parameters;
};
}
    
#endif // IDEAL_GENERIC_CAMERA_MODEL_HPP
