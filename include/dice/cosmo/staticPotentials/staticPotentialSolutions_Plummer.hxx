#ifndef __STATIC_POTENTIAL_SOLUTIONS_PLUMMER_HXX__
#define __STATIC_POTENTIAL_SOLUTIONS_PLUMMER_HXX__

#include "staticPotentialSolutionsInterface.hxx"

#include "../../internal/namespace.header" 

namespace cosmo {

  template <int ND>
  class StaticPotentialSolutions_PlummerT : 
    public StaticPotentialSolutionsInterfaceT<ND>
  {
  public:
    typedef StaticPotentialSolutionsInterfaceT<ND> Base;

     static const int NDIM=ND;

    template <class R, class PM>
    StaticPotentialSolutions_PlummerT(R *reader, PM &paramsManager, double mass, double G_)
    {
      initialize(reader,paramsManager,mass,G_);
    }
  
    double acc_over_r(int i, double r, double *coords) const
    {
      return cOut*pow(r*r+sphereRadius*sphereRadius,-1.5); 
    }

    double density(double r) const
    {
      return sphereDensity * pow(1.+(r*r)/(sphereRadius*sphereRadius),-2.5);
    }
  
    template <class R, class PM>
    void initialize(R *reader, PM &paramsManager,double mass, double G_)
    {
      sphereRadius = 0.2;
      sphereRadius = paramsManager.
	get("radius",Base::parserCategory(),sphereRadius,reader,
	    PM::FILE_FIRST,
	    "Effective radius of the static potential");

      initialize(mass,G_);
    }

  private:

    void initialize(double mass, double G_)
    {
      const double pi = 4.0*atan(1.0);

      G=G_;
      sphereMass=mass;
      cIn = -G*sphereMass/pow(sphereRadius,3.0);
      cOut = -G*sphereMass;
      if (NDIM==3)
	sphereVolume  = 4.0/3.0 * pi * pow(sphereRadius,3.0);
      else
	sphereVolume  = 4.0*pi*pow(sphereRadius,2.0);
      sphereDensity = sphereMass/sphereVolume;
    }

    double sphereMass;
    double sphereRadius;
    double sphereVolume;
    double sphereDensity;
    double G;
    double cIn;
    double cOut;
  };
}

#include "../../internal/namespace.footer"
#endif
