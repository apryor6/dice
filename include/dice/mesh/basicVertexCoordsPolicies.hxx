#ifndef __BASIC_VERTEX_COORDS_POLICIES_HXX__
#define __BASIC_VERTEX_COORDS_POLICIES_HXX__

#include <set>

#include "../algebra/surfaceFitter.hxx"
#include "../finiteElements/quadraticSimplex.hxx"

#ifdef HAVE_BOOST
#ifdef HAVE_QUADMATH
#include <boost/multiprecision/float128.hpp>
#endif
#endif

/**
 * @file 
 * @brief Default policies for computing the coordinates of newly created vertices when a 
 * simplex is refined.
 * @author Thierry Sousbie
 */

#include "../internal/namespace.header"

/** \addtogroup MESH
 *   \{
 */

/**
 * \namespace vertexRefineCoordsPolicy
 * \brief classes that define policies for computing the coordinates of newly created vertices when a simplex is refined. To be used as MeshTraitsT template parameter.
 */

namespace vertexRefineCoordsPolicy {
  
  /** \class Dummy 
   * \brief does nothing ;) 
   */      
  template <typename T, class M, class SEG, class V, class S>
  struct Dummy
  {    
    static const int SIMPLEX_BUFFER_SIZE = 0;

    static void refine(T *result, const M *mesh, const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer){}
  };

  /** \class MidPoint 
   * \brief center of the refined segment 
   */      
  template <typename T, class M, class SEG, class V, class S>
  struct MidPoint
  {   
    static const int SIMPLEX_BUFFER_SIZE = 0;

    static void refine(T *result, const M *mesh, const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {   
      const T *v0 = seg->getVertex(0)->getCoordsConstPtr();
      const T *v1 = seg->getVertex(1)->getCoordsConstPtr();
      mesh->getGeometry()->midPointCoords(v0,v1,result);
    }
  };

  /** \class TracersBarycenter
   *  \brief barycenter of simplex tracers (a 'tracer' simplex data field must be defined !)
   */      
  template <typename T, class M, class SEG, class V, class S>
  struct TracersBarycenter
  {   
    static const int SIMPLEX_BUFFER_SIZE = 0;

    static void refine(T *result, const M *mesh, const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {       
      const double factor = 1.0L/nSimplices;
      const double *refCoords=simplices[0]->tracer.getPointer();    
      
      std::copy(refCoords,refCoords+V::NDIM_W,result);

      for (int i=1;i<nSimplices;++i)
	{
	  const double *coords=simplices[i]->tracer.getPointer();  
	  for (int j=0;j<V::NDIM_W;++j)
	    result[j]+=mesh->getGeometry()->checkCoordConsistency(coords[j],refCoords[j],j);
	}

      for (int i=0;i<V::NDIM_W;++i) result[i]*=factor;
      mesh->getGeometry()->checkBoundary(result);     
    }
  };

  /** \class LinearRegressionWithTracerAndSegTracers
   * \brief use segtracer as refined coordinates of the new vertex and linearly interpolate
   * the coordinates of the new tracers
   * \pre 'segTracers' and 'tracer'simplex data field must be defined    
   */
  template <typename T, class M, class SEG, class V, class S>
  struct LinearRegressionWithTracers
  { 
    // This is the size of the buffer we will use to pass the result 
    // to the simplex/segment tracers
    static const long SIMPLEX_BUFFER_SIZE = 
      2*S::NDIM_W*sizeof(typename S::SegTracers::Type)+
      S::NVERT*S::NDIM_W*sizeof(typename S::SegTracers::Type)+
      (S::NVERT+1)*sizeof(V*);
      
    static const int NDIM=S::NDIM;
    static const int NDIM_W=S::NDIM_W;
        
    typedef typename S::Neighborhood Neighborhood;
    typedef typename V::Coord Coord;    
    typedef typename S::SegTracers::Type Tracer;

    typedef double CT;
  
    static void refine(T *result, const M *mesh, 
		       const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {     
      typename M::GeometricProperties *geometry = mesh->getGeometry();  
      // Seg tracer becomes the new vertex
      int si=simplices[0]->findSegmentIndex(seg);
      std::copy_n(simplices[0]->segTracers.getPointer() + (si*NDIM_W),
		  NDIM_W,result);
      mesh->getGeometry()->checkBoundary(result);
      
      // compute the new tracers coordinates and put them into the buffer 
      for (int i=0;i<nSimplices;++i)
	{	  	 
	  S* simplex = simplices[i];
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  // Buffer to simplex tracers
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  // Buffer to segment tracers
	  Tracer *segTBuffer = &tBuffer[2*NDIM_W];
	  // Buffer to vertex pointer (used to identify segment tracers)
	  V **vIdBuffer=static_cast<V**>(static_cast<void*>(&segTBuffer[S::NVERT*NDIM_W]));
	  
	  // p[i][] contains the vertices of the simplex containing the ith vertex of 
	  // the split segment
	  const Coord *p[2][S::NVERT];
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V *v=simplex->getVertex(j);
	      if (v==seg->getVertex(0))
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=result;	
		}
	      else if (v==seg->getVertex(1))
		{
		  p[1][j]=v->getCoordsPtr();
		  p[0][j]=result;			
		}
	      else
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=v->getCoordsPtr();	
		}
	    }

	  // n is the index of the split simplex (formed by the p[n][] vertices)
	  for (int n=0;n<2;++n)
	    {
	      Coord point[NDIM_W];	 

	      // New simplex tracer coordinate : barycenter	      
	      std::fill_n(point,NDIM_W,0);      
	      for (int j=0;j<S::NVERT;++j) 
		{
		  for (int k=0;k<NDIM_W;++k)
		    point[k] += geometry->
		      checkCoordConsistency(p[n][j][k],result[k],k);
		}

	      for (int j=0;j<NDIM_W;++j) 
		point[j]/=S::NVERT;

	      mesh->getGeometry()->checkBoundary(point);

	      for (int j=0;j<NDIM_W;++j) 
		tBuffer[n*NDIM_W+j]=point[j];

	      if (n==0) vIdBuffer[S::NVERT]=seg->getVertex(0);
	    }
	  
	  // New segment tracers coordinate:
	  // segment mid-point or unchanged
	  for (int j=0;j<S::NVERT;++j)
	    {
	      Coord point[NDIM_W];
	      V* vertex=simplex->getVertex(j);
	      const Coord *p2=vertex->getCoordsConstPtr();
	      for (int k=0;k<NDIM_W;++k) 
		{
		  point[k]  = result[k];
		  point[k] += geometry->checkCoordConsistency(p2[k],result[k],k);
		  point[k] *= 0.5;		 
		}
	      mesh->getGeometry()->checkBoundary(point);
	      for (int k=0;k<NDIM_W;++k) segTBuffer[j*NDIM_W+k]=point[k];

	      // store the opposite vertex address necessary to identify the segment
	      vIdBuffer[j]=vertex;
	    }	 
	}
    }
  };

  template <typename T, class M, class SEG, class V, class S>
  struct QuadraticRegressionWithTracers
  { 
    // This is the size of the buffer we will use to pass the result 
    // to the simplex/segment tracers
    static const long SIMPLEX_BUFFER_SIZE = 
      2*S::NDIM_W*sizeof(typename S::SegTracers::Type)+
      S::NVERT*S::NDIM_W*sizeof(typename S::SegTracers::Type)+
      (S::NVERT+1)*sizeof(V*);
      
    static const int NDIM=S::NDIM;
    static const int NDIM_W=S::NDIM_W;
        
    typedef typename S::Neighborhood Neighborhood;
    typedef typename V::Coord Coord;    
    typedef typename S::SegTracers::Type Tracer;

    typedef double CT;    
    //typedef long double CT;
    //typedef boost::multiprecision::float128 CT;

    typedef QuadraticSimplexT<NDIM,NDIM_W,CT> QSimplex;
  
    static void refine(T *result, const M *mesh, 
		       const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {     
      typename M::GeometricProperties *geometry = mesh->getGeometry();
      // A map used to identify new segments from their vertices
      std::map<V*,int> vertexMap;       
      // Storage for contributions from different simplices fit to new tracer coordinates
      std::vector<Coord> contribCoord;
      std::vector<int> contribCount;

      // That should be the upper limit 
      contribCoord.reserve((nSimplices+3)*NDIM_W);
      contribCount.reserve(nSimplices+3);

      // The segTracer of the split segment becomes the new vertex
      int si=simplices[0]->findSegmentIndex(seg);
      std::copy_n(simplices[0]->segTracers.getPointer() + (si*NDIM_W),
		  NDIM_W,result);
      geometry->checkBoundary(result);      
  
      // compute the new tracers coordinates and put them into the buffer 
      for (int i=0;i<nSimplices;++i)
	{	  	 
	  S* simplex = simplices[i];
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  // Buffer to simplex tracers
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  // Buffer to segment tracers
	  Tracer *segTBuffer = &tBuffer[2*NDIM_W];
	  // Buffer to vertex pointer (used to identify segment tracers)
	  V **vIdBuffer=static_cast<V**>(static_cast<void*>(&segTBuffer[S::NVERT*NDIM_W]));

	  // First set the vertices and tracers coordinates used for the quadratic simplex
	  QSimplex qSimplex(simplex,simplex->segTracers.getConstPointer(),geometry);
	 
	  // We'll need the index of the split segment in the current simplex
	  int segmentIndex = simplex->findSegmentIndex(seg);
	  std::pair<int,int> segVertId=S::getSegmentVerticesIndex(segmentIndex);

	  // split segment tracer barycentric coordinates
	  CT tBC[S::NVERT]={0};
	  tBC[segVertId.first]=0.5;
	  tBC[segVertId.second]=0.5;
	  
	  // n is the index of the split simplex 
	  // -> compute the new simplex tracers coordinates
	  for (int n=0;n<2;++n)
	    {	    
	      CT bC[NDIM+1]; //barycentric coordinates
	      Coord cE[NDIM_W];
	      
	      if (n==0) 
		{
		  bC[segVertId.first]=1.5/S::NVERT;
		  bC[segVertId.second]=0.5/S::NVERT;
		}
	      else
		{
		  bC[segVertId.second]=1.5/S::NVERT;
		  bC[segVertId.first]=0.5/S::NVERT;
		}
	      
	      for (int j=0;j<S::NVERT;++j)
		{
		  if ((j!=segVertId.first)&&
		      (j!=segVertId.second))
		    bC[j]=1.0/S::NVERT;
		}	      	  	     

	      // and get the new simplex tracer
	      qSimplex.barycentricToPosition(bC,cE);	      
	      mesh->getGeometry()->checkBoundary(cE);

	      for (int j=0;j<NDIM_W;++j) 
		tBuffer[n*NDIM_W+j]=cE[j]; // That's where it goes !

	      if (n==0) vIdBuffer[S::NVERT]=simplex->getVertex(segVertId.first);
	    }	  

	  // -> compute the new segment tracers coordinates
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V* vertex=simplex->getVertex(j);
	      CT bC[NDIM+1]; //barycentric coordinates
	      Coord cE[NDIM_W];

	      // First get its barycentric coordinates
	      std::copy_n(tBC,S::NVERT,bC);
	      bC[j]+=1.0;	     
	      for (int k=0;k<S::NVERT;++k) 
		bC[k]/=2;
	      
	      // fit the new segment tracer	      
	      qSimplex.barycentricToPosition(bC,cE);
	      
	      // Now add this fit's coordinates to the contributions for this seg tracer
	      int index;
	      auto it= vertexMap.find(vertex);
	      if (it==vertexMap.end()) 
		{
		  index = vertexMap.size();
		  vertexMap.insert(std::make_pair(vertex,index));

		  contribCount.push_back(1);
		  
		  for (int k=0;k<NDIM_W;++k)
		    contribCoord.
		      push_back(geometry->checkCoordConsistency(cE[k],result[k],k));
		}
	      else 
		{
		  index=it->second;
		  contribCount[index]++;
		  		  
		  for (int k=0;k<NDIM_W;++k)
		    contribCoord[index*NDIM_W + k]+=geometry->
		      checkCoordConsistency(cE[k],result[k],k);
		}

	      // We could store the new tracer coordinates now but will average them 
	      // over different fits contributions instead. So we store the index of the
	      // contribution instead to avoid wasting time trying to retrieve it later
	      segTBuffer[j*NDIM_W]=index;

	      // store the opposite vertex address necessary to identify the segment
	      vIdBuffer[j]=vertex;	     
	    }	  
	}

      // Now fits should in theory be continuous on the boundaries, however this is 
      // not exactly the case numerically so we average any new tracer coordinate over all
      // their contributions
      for (unsigned long i=0;i<contribCount.size();i++)
	{	  
	  double factor = 1.0 / contribCount[i];
	  for (int j=0;j<NDIM_W;++j)
	    contribCoord[i*NDIM_W+j]*=factor;
	  geometry->checkBoundary(&contribCoord[i*NDIM_W]);	
	}
      
      // And finally, store the correct coordinates in the buffer
      for (int i=0;i<nSimplices;++i)
	{
	  // Pointer to start of buffer for this simplex
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  // Buffer to simplex tracers
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  // Segment tracers buffer
	  Tracer *segTBuffer = &tBuffer[2*NDIM_W];

	  for (int j=0;j<S::NVERT;++j)
	    {
	      int index = NDIM_W * static_cast<int>(segTBuffer[j*NDIM_W]);
	      for (int k=0;k<NDIM_W;++k) 
		segTBuffer[j*NDIM_W+k]=contribCoord[index+k];// That's where it goes !
	    }
	}
            
    }
  };


  template <typename T, class M, class SEG, class V, class S>
  struct QuadraticRegressionWithTracerAndSegTracers_BAD
  { 
    // This is the size of the buffer we will use to pass the result 
    // to the simplex/segment tracers
    static const long SIMPLEX_BUFFER_SIZE = 
      2*S::NDIM_W*sizeof(typename S::Tracer::Type)+
      S::NVERT*S::NDIM_W*sizeof(typename S::Tracer::Type)+
      S::NVERT*sizeof(V*);
      
    static const int NDIM=S::NDIM;
    static const int NDIM_W=S::NDIM_W;
        
    typedef typename S::Neighborhood Neighborhood;
    typedef typename V::Coord Coord;    
    typedef typename S::Tracer::Type Tracer;

    typedef double CT;    
    //typedef long double CT;
    //typedef boost::multiprecision::float128 CT;

    typedef SurfaceFitterT<NDIM,NDIM+NDIM_W,2,CT> SurfaceFitter;
    typedef typename SurfaceFitter::FitFunctor Fit;

    static void refine(T *result, const M *mesh, 
		       const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {     
      typename M::GeometricProperties *geometry = mesh->getGeometry();
      // A map used to identify new segments from their vertices
      std::map<V*,int> vertexMap;       
      // Storage for contributions from different simplices fit to new tracer coordinates
      std::vector<Coord> contribCoord;
      std::vector<int> contribCount;

      // That should be the upper limit 
      contribCoord.reserve((nSimplices+3)*NDIM_W);
      contribCount.reserve(nSimplices+3);

      // The segTracer of the split segment becomes the new vertex
      int si=simplices[0]->findSegmentIndex(seg);
      std::copy_n(simplices[0]->segTracers.getPointer() + (si*NDIM_W),
		  NDIM_W,result);
      geometry->checkBoundary(result);

      Coord points[S::NVERT+S::NSEG+1][NDIM+NDIM_W]={0};      
     
      // Arbitrary lagrangian coordinates : a regular simplex so that there is no
      // privileged vertex
      // The vertices
      points[1][0]=1.0;
      if (NDIM>1)
	{
	  points[2][0]=0.5;
	  points[2][1]=8.6602540378443860e-01; //sqrt(0.75);
	}
      if (NDIM>2)
	{
	  points[3][0]=0.5;
	  points[3][1]=2.8867513459481287e-01; //sqrt(1.0/12.0);
	  points[3][2]=8.1649658092772603e-01; //sqrt(2.0/3.0);
	}
      // The segment tracers
      for (int i=0;i<S::NSEG;++i) 
	{
	  std::pair<int,int> vid=S::getSegmentVerticesIndex(i);
	  const Coord *p1=points[vid.first];
	  const Coord *p2=points[vid.second];
	  for (int j=0;j<NDIM;++j)
	    points[S::NVERT+i][j]=(p1[j]+p2[j])/2;
	}
      // The simplex tracer
      for (int i=0;i<S::NVERT;++i)
	for (int j=0;j<NDIM;++j)
	  points[S::NVERT+S::NSEG][j]+=points[i][j];

      for (int j=0;j<NDIM;++j)
	points[S::NVERT+S::NSEG][j]/=S::NVERT;

      /*
      // Same thing but with non regular simplex ...
      
      // The simplex vertices (arbitrary lagrangian coordinates)
      for (int i=1;i<S::NVERT;++i) points[i][i-1]=1;
      // The segment tracers
      for (int i=0;i<S::NSEG;++i) 
	{
	  std::pair<int,int> vid=S::getSegmentVerticesIndex(i);
	  const Coord *p1=points[vid.first];
	  const Coord *p2=points[vid.second];
	  for (int j=0;j<NDIM;++j)
	    points[S::NVERT+i][j]=(p1[j]+p2[j])/2;
	}
      // The simplex tracer
      for (int j=0;j<NDIM;++j)
	points[S::NVERT+S::NSEG][j]=1.0/S::NVERT;
      */
      // compute the new tracers coordinates and put them into the buffer 
      for (int i=0;i<nSimplices;++i)
	{	  	 
	  S* simplex = simplices[i];
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  // Buffer to simplex tracers
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  // Buffer to segment tracers
	  Tracer *segTBuffer = &tBuffer[2*NDIM_W];
	  // Buffer to vertex pointer (used to identify segment tracers)
	  V **vIdBuffer=static_cast<V**>(static_cast<void*>(&segTBuffer[S::NVERT*NDIM_W]));
	  	  
	  // First get the vertices and tracers coordinates for the fit
	  // the simplex vertices
	  for (int j=0;j<S::NVERT;++j)
	    simplex->getVertex(j)->getCoords(&points[j][NDIM]);

	  // and the segment tracers
	  const Tracer* c=simplex->segTracers.getConstPointer();
	  for (int j=0;j<S::NSEG;++j,c+=NDIM_W)
	    std::copy_n(c,NDIM_W,&points[S::NVERT+j][NDIM]);
		  
	  SurfaceFitter sf;
	  // We do not add the simplex tracer !
	  sf.addPoints(&points[0],&points[S::NVERT+S::NSEG+1]);
	  
	  // create the fit
	  Fit fit = sf.fit();
	 
	  // We'll need the index of the split segment in the current simplex
	  int segmentIndex = simplex->findSegmentIndex(seg);  

	  // n is the index of the split simplex 
	  // -> compute the new simplex tracers coordinates
	  for (int n=0;n<2;++n)
	    {	      
	      V *sV=seg->getVertex(n);
	      Coord cE[NDIM_W];	 
	      Coord cL[NDIM]={0};
	      
	      // Compute lagrangian coordinates of the tracer
	      for (int j=0;j<S::NVERT;++j) 
		{
		  if (simplex->getVertex(j) == sV)
		    {
		      for (int k=0;k<NDIM;++k)
			cL[k] += points[S::NVERT+segmentIndex][k];	
		    }
		  else
		    {
		      for (int k=0;k<NDIM;++k)
			cL[k] += points[j][k];		    
		    }
		}

	      for (int j=0;j<NDIM;++j) 
		cL[j]/=S::NVERT;

	      // and fit the new simplex tracer
	      fit.getValue(cL,cE);
	      mesh->getGeometry()->checkBoundary(cE);

	      for (int j=0;j<NDIM_W;++j) 
		tBuffer[n*NDIM_W+j]=cE[j]; // That's where it goes !		
	    }	  
	  
	  // -> compute the new segment tracers coordinates
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V* vertex=simplex->getVertex(j);
	      
	      Coord cL[NDIM];
	      Coord cE[NDIM_W];

	      // First get its lagrangian coords
	      for (int k=0;k<NDIM;++k)
		{
		  cL[k] = 0.5 * 
		    (points[S::NVERT+segmentIndex][k]+ // segment tracer lag. coordinate
		     points[j][k]); // vertex lag. coordinate		  
		}
	      
	      // fit the new segment tracer
	      fit.getValue(cL,cE);	      	   	      
	      const Tracer* c=&simplex->segTracers.getConstPointer()[NDIM_W*segmentIndex];
	      printf("Tracer @ (%e %e %e %e) / (%e %e %e %e) \n",
		     vertex->getCoord(0),vertex->getCoord(1),
		     vertex->getCoord(2),vertex->getCoord(3),
		     c[0],c[1],c[2],c[3]);

	      // Now add this fit's coordinates to the contributions for this seg tracer
	      int index;
	      auto it= vertexMap.find(vertex);
	      if (it==vertexMap.end()) 
		{
		  index = vertexMap.size();
		  vertexMap.insert(std::make_pair(vertex,index));

		  contribCount.push_back(1);
		  
		  for (int k=0;k<NDIM_W;++k)
		    contribCoord.
		      push_back(geometry->checkCoordConsistency(cE[k],result[k],k));

		  index *= NDIM_W;
		  printf("Creating @%2.2d: (%g %g) -> (%g %g %g %g) => (%g %g %g %g)\n",
			 index/NDIM_W,
			 cL[0],cL[1],
			 contribCoord[index+0],contribCoord[index+1],
			 contribCoord[index+2],contribCoord[index+3],
			 contribCoord[index+0],contribCoord[index+1],
			 contribCoord[index+2],contribCoord[index+3]);
		}
	      else 
		{
		  index=it->second;
		  contribCount[index]++;
		  index *= NDIM_W;		  
		  for (int k=0;k<NDIM_W;++k)
		    contribCoord[index + k]+=geometry->
		      checkCoordConsistency(cE[k],result[k],k);

		  printf("Adding   @%2.2d: (%g %g) -> (%g %g %g %g) => (%g %g %g %g)\n",
			 index/NDIM_W,
			 cL[0],cL[1],
			 geometry->checkCoordConsistency(cE[0],result[0],0),
			 geometry->checkCoordConsistency(cE[1],result[1],0),
			 geometry->checkCoordConsistency(cE[2],result[2],0),
			 geometry->checkCoordConsistency(cE[3],result[3],0),
			 contribCoord[0],contribCoord[1],contribCoord[2],contribCoord[3]);
		}

	      // We should store the new tracer coordinates now but will average them 
	      // over different fits contributions instead. So we store the index of the
	      // contribution instead to avoid wasting time trying to retrieve it later
	      segTBuffer[j*NDIM_W]=index;

	      // store the opposite vertex address necessary to identify the segment
	      vIdBuffer[j]=vertex;	      
	    }	  
	}

      // Now fits should in theory be continuous on the boundaries, however this is 
      // not exactly the case numerically so we average any new tracer coordinate over all
      // their contributions
      for (unsigned long i=0;i<contribCount.size();i++)
	{
	  double factor = 1.0 / contribCount[i];
	  for (int j=0;j<NDIM_W;++j)
	    contribCoord[i*NDIM_W+j]*=factor;
	  geometry->checkBoundary(&contribCoord[i*NDIM_W]);
	}

      // And finally, store the correct coordinates in the buffer
      for (int i=0;i<nSimplices;++i)
	{
	  // Pointer to start of buffer for this simplex
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  // Buffer to simplex tracers
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  // Segment tracers buffer
	  Tracer *segTBuffer = &tBuffer[2*NDIM_W];

	  for (int j=0;j<S::NVERT;++j)
	    {
	      int index = NDIM_W * static_cast<int>(segTBuffer[j*NDIM_W]);
	      for (int k=0;k<NDIM_W;++k) 
		segTBuffer[j*NDIM_W+k]=contribCoord[index+k];// That's where it goes !
	    }
	}
      
    }
  };

  
  
  /** \class QuadraticRegressionWithTracer
   * \brief projection onto a quadratic regression of the mesh of the midpoint. 
   * \pre 'tracer' simplex data field must be defined and have a 
   * simplexRefineDataPolicy::QuadraticRegressionWithTracer!
   */  
  template <typename T, class M, class SEG, class V, class S>
  struct QuadraticRegressionWithTracer
  {   
    static const long SIMPLEX_BUFFER_SIZE = 
      sizeof(V*) +
      S::NDIM_W*2*sizeof(typename S::Tracer::Type);

    static const int NDIM=S::NDIM;
    static const int NDIM_W=S::NDIM_W;
    
    typedef typename S::Neighborhood Neighborhood;
    typedef typename V::Coord Coord;    
    typedef typename S::Tracer::Type Tracer;

    typedef SurfaceFitterT<NDIM,NDIM_W,2,Coord> SurfaceFitter;    
    typedef typename SurfaceFitter::FitFunctor Fit;

    template <class G, typename T1, typename T2, typename T3>
    static void getCoordinates(G *geometry, T1* ref, T2* in, T3 *out)
    {
      for (int j=0;j<NDIM_W;++j)
	out[j]=geometry->correctCoordsDiff(in[j]-ref[j],j);
    }

    static void refine(T *result, const M *mesh, 
		       const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {  
      typename M::GeometricProperties *geometry = mesh->getGeometry();      
      Fit fit[nSimplices];
      Coord origin[nSimplices][NDIM_W];      
      
      T midPoint[NDIM_W];           
      geometry->midPointCoords(seg->getVertex(0)->getCoordsConstPtr(),
			       seg->getVertex(1)->getCoordsConstPtr(),
			       midPoint);      
      
      // position of the barycenter of the 'nSimplices' simplices
      double barycenter[NDIM_W]; 
      // position of the barycenter advected since last refinement
      double advectedMidPoint[NDIM_W]; 

      std::fill_n(barycenter,NDIM_W,0);
      std::fill_n(advectedMidPoint,NDIM_W,0);
      
      
      for (int i=0;i<nSimplices;++i)
	{
	  // origin[i] is the barycenter of simplex 'i'
	  std::fill_n(origin[i],NDIM_W,0);    

	  for (int j=0;j<S::NVERT;++j) 
	    {
	      const Coord *coords=simplices[i]->getVertex(j)->getCoordsConstPtr();
	  
	      for (int k=0;k<NDIM_W;++k)
		origin[i][k] += geometry->
		  checkCoordConsistency(coords[k],midPoint[k],k);	      
	    }
	  for (int j=0;j<NDIM_W;++j) origin[i][j]/=S::NVERT;
	  
	  
	  // deformation is computed as the displacement of tracers W.R.T the barycenter
	  // of their simplex (i.e. within the 'mesh' coordinates)
	  double deformation[NDIM_W];
	  getCoordinates(geometry,origin[i],
			 simplices[i]->tracer.getPointer(),
			 deformation);

	  for (int j=0;j<NDIM_W;++j) 
	    {
	      barycenter[j]+=origin[i][j];
	      //advectedMidPoint[j]+=deformation[j];
	    }
	  
	}
      
      for (int j=0;j<NDIM_W;++j)
	{
	  barycenter[j] /= nSimplices;
	  advectedMidPoint[j] /= nSimplices;
	  // Add average deformation vector to the barycenter to get the advected position
	  advectedMidPoint[j] += midPoint[j];
	}
      

      std::fill_n(result,NDIM_W,0);
      int debugI=-1;
      for (int i=0;i<nSimplices;++i)
	 {
	   SurfaceFitter sf;
	   Coord base[NDIM][NDIM_W];	   
	   Coord point[NDIM_W];
	   
	   bool debug=false;
	   /*
	   if (simplices[i]->getLocalIndex() == 4029) 
	     {
	       debugI=i;debug=true;
	       printf("\nDebugging simplex %ld\n",(long)simplices[i]->getLocalIndex());
	     }
	      
	   if (simplices[i]->getLocalIndex() == 4289) 
	     {
	       debugI=i;debug=true;
	       printf("\nDebugging simplex %ld\n",(long)simplices[i]->getLocalIndex());
	     }

	   if (simplices[i]->getLocalIndex() == 4028) 
	     {
	       debugI=i;debug=true;
	       printf("\nDebugging simplex %ld\n",(long)simplices[i]->getLocalIndex());
	     }
	      
	   if (simplices[i]->getLocalIndex() == 4288) 
	     {
	       debugI=i;debug=true;
	       printf("\nDebugging simplex %ld\n",(long)simplices[i]->getLocalIndex());
	     }
	   */
	     
	   // Set the face barycenter as the origin of the tangent space. All coordinates 
	   // must have this point as origin, so we'll use getCoordinates().
	   /*
	   std::fill_n(origin[i],NDIM_W,0);      
	   for (int j=0;j<S::NVERT;++j) 
	     {
	       const Coord *coords=simplices[i]->getVertex(j)->getCoordsConstPtr();
	  
	       for (int k=0;k<NDIM_W;++k)
		 origin[i][k] += geometry->
		   checkCoordConsistency(coords[k],barycenter[k],k);
	     }
	   for (int j=0;j<NDIM_W;++j) origin[i][j]/=S::NVERT;
	   */

	   /*
	   // Set the simplex tracer as origin	   
	   for (int j=0;j<NDIM_W;++j)
	     origin[i][j] = geometry->checkCoordConsistency
	       (simplices[i]->tracer.getValueAt(j),midPoint[j],j);
	   */
	   	   
	   /*
	   const Coord *p[S::NVERT];
	   simplices[i]->getVerticesCoordsConstPtr(p);
	   //std::swap(p[0],p[1]);
	   geometry->template getBaseVectors<Coord,Coord,S::NVERT-1,NDIM_W>(p,base);    
	   */
	   	 	   

	   // Add the tracer (+1 point)	   
	   getCoordinates(geometry,origin[i],simplices[i]->tracer.getPointer(),point);
	   sf.addPoint(point);

	   Coord vCoord[S::NVERT][NDIM_W];
	   // Add vertices (+4 points)	 
	   for (int j=0;j<S::NVERT;++j)
	     {
	       getCoordinates(geometry,origin[i],
			      simplices[i]->getVertex(j)->getCoordsPtr(),
			      vCoord[j]);	       
	       sf.addPoint(vCoord[j]);
	     }

	   // And all the neighbors tracers + non-shared vertices (+8 points at most)
	   // Don't forget to remove possible duplicates
	   std::set<Coord *> vertices; 
	   Neighborhood nb;
	   simplices[i]->getNeighborhood(nb);
	   for (int j=0;j<S::NNEI;++j)
	     {
	       if (nb.neiS[j]!=NULL)
		 {
		   vertices.insert(nb.neiV[j]->getCoordsPtr());
		   //vertices.insert(nb.neiS[j]->tracer.getPointer());
		   getCoordinates(geometry,origin[i],
				  nb.neiS[j]->tracer.getPointer(),point);
		   sf.addPoint(point);		   
		 }
	     }

	   typedef typename std::set<Coord *>::iterator Set_it;
	   for (Set_it it=vertices.begin();it!=vertices.end();++it)
	     {
	       getCoordinates(geometry,origin[i],(*it),point);
	       sf.addPoint(point);
	     }	  
	   
	   // Check whether we have an under-determined system (need 10 points)
	   if (sf.getPointsCount() < SurfaceFitter::NCOEFS)
	     {
	       PRINT_SRC_INFO(LOG_ERROR);
	       glb::console->print<LOG_ERROR>
		 ("System is under-determined, this case is not implemented yet !\n");
	       exit(-1);
	     }
	   
	   // First guess : set tangent space base as the simplex axes
	   mesh->getBaseVectors(simplices[i],base);
	   sf.setTangentSpaceBase(base,debug);
	   //double cond;
	   fit[i]=sf.fit();//debug);

	   // The initial tangent base we used (i.e. simplex axes) may not be a very 
	   // good guess so refit the points using the tangent base deduced from the fit
	   // at the simplices vertices
	   //sf.refit(fit[i],vCoord,S::NVERT);

	   // Note that midPoint will be projected to the new tangent base 
	   // first so it is fine even if it does not belong to the subspace it spans ...
	   double pSplitPoint[NDIM_W];
	   getCoordinates(geometry,origin[i],advectedMidPoint,point);
	   fit[i].projectToSurface(point,pSplitPoint,debug);

	   for (int j=0;j<NDIM_W;++j)
	     result[j]+=pSplitPoint[j]+origin[i][j];

	   if (debug)
	     {
	       printf("Origin : ");
	       for (int j=0;j<NDIM_W;++j)
		 printf("%lg ",origin[i][j]);
	       printf("\n");
	       printf("InterpCoord : ");
	       for (int j=0;j<NDIM_W;++j)
		 printf("%lg ",pSplitPoint[j]+origin[i][j]);
	       printf("\n");
	     }
	 }	  
      
      double fac = 1.0L / nSimplices;
      for (int i=0;i<NDIM_W;++i)
	result[i]*=fac;

      if (debugI>-1)
	{
	  printf("NewCoord : ");
	  for (int j=0;j<NDIM_W;++j)
	    printf("%lg ",result[j]);
	  printf("\n");
	}
    
      // Now compute the new tracers coordinates and put them into the buffer 
      // so that we can use them to refine simplices data
      // This versions computes new fits in the refined simplices base (slower but more
      // accurate as the new base is closer to the tangent space ...)
      for (int i=0;i<nSimplices;++i)
	{	  	 
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  	  	  
	  const Coord *p[2][S::NVERT];
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V *v=simplices[i]->getVertex(j);
	      if (v==seg->getVertex(0))
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=result;	
		}
	      else if (v==seg->getVertex(1))
		{
		  p[1][j]=v->getCoordsPtr();
		  p[0][j]=result;			
		}
	      else
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=v->getCoordsPtr();	
		}
	    }
	  
	  for (int n=0;n<2;++n)
	    {
	      SurfaceFitter sf;
	      Coord org[NDIM_W];
	      Coord point[NDIM_W];

	      Coord base[NDIM][NDIM_W];
	      geometry->template getBaseVectors<Coord,Coord,S::NVERT-1,NDIM_W>(p[n],base); 
	      sf.setTangentSpaceBase(base);

	      std::fill_n(org,NDIM_W,0);      
	      for (int j=0;j<S::NVERT;++j) 
		{
		  //const Coord *coords=simplices[i]->getVertex(j)->getCoordsConstPtr();
	      
		  for (int k=0;k<NDIM_W;++k)
		    org[k] += geometry->
		      checkCoordConsistency(p[n][j][k],midPoint[k],k);
		}
	      for (int j=0;j<NDIM_W;++j) org[j]/=S::NVERT;

	      getCoordinates(geometry,org,simplices[i]->tracer.getPointer(),point);
	      sf.addPoint(point);

	      getCoordinates(geometry,org,result,point);
	      sf.addPoint(point);

	      Coord vCoord[S::NVERT][NDIM_W];
	      for (int j=0;j<S::NVERT;++j)
		{
		  getCoordinates(geometry,org,
				 simplices[i]->getVertex(j)->getCoordsPtr(),
				 vCoord[j]);	       
		  sf.addPoint(vCoord[j]);
		}
	      
	      std::set<Coord *> vertices; 
	      Neighborhood nb;
	      simplices[i]->getNeighborhood(nb);
	      for (int j=0;j<S::NNEI;++j)
		{
		  if (nb.neiS[j]!=NULL)
		    {
		      vertices.insert(nb.neiV[j]->getCoordsPtr());
		      //vertices.insert(nb.neiS[j]->tracer.getPointer());
		      getCoordinates(geometry,org,
				     nb.neiS[j]->tracer.getPointer(),point);
		      sf.addPoint(point);		   
		    }
		}

	      typedef typename std::set<Coord *>::iterator Set_it;
	      for (Set_it it=vertices.begin();it!=vertices.end();++it)
		{
		  getCoordinates(geometry,org,(*it),point);
		  sf.addPoint(point);
		}	 
	      
	      // We may not need to refit the base here ...
	      //double cond;
	      Fit fit=sf.fit();	      
	      //sf.refit(fit,vCoord,S::NVERT);

	      std::fill_n(point,NDIM_W,0);
	      fit.projectToSurface(point,&tBuffer[n*NDIM_W]);
	      for (int j=0;j<NDIM_W;++j) tBuffer[n*NDIM_W+j]+=org[j];
	    }

	  // This is used to recognize which tracer belongs to which split simplex
	  void *b = &tBuffer[2*NDIM_W];
	  V** vBuffer = static_cast<V**>(b);
	  (*vBuffer)=seg->getVertex(0);    	  
	}

      mesh->getGeometry()->checkBoundary(result);
    }
  };

  /** \class QuadraticRegressionWithTracerAndSegTracers
   * \brief use segtracer as refined coordinates of the new vertex and compute new tracers
   * coordinates by projecting midpoints onto a quadratic regression of the mesh surface.
   * \pre 'segTracers' and 'tracer'simplex data field must be defined  
   * \warning Regression is not robust enough, so this will crash at some point ...
   */ 
  template <typename T, class M, class SEG, class V, class S>
  struct QuadraticRegressionWithTracerAndSegTracers_OLD
  {   
    // This is the size of the buffer we will use to pass the result 
    // to the simplex/segment tracers
    static const long SIMPLEX_BUFFER_SIZE = 
      2*S::NDIM_W*sizeof(typename S::Tracer::Type)+
      S::NVERT*S::NDIM_W*sizeof(typename S::Tracer::Type)+
      S::NVERT*sizeof(V*);
    
    static const int NDIM=S::NDIM;
    static const int NDIM_W=S::NDIM_W;
        
    typedef typename S::Neighborhood Neighborhood;
    typedef typename V::Coord Coord;    
    typedef typename S::Tracer::Type Tracer;

    typedef double CT;
    //typedef long double CT;

    //typedef boost::multiprecision::float128 CT;
    typedef SurfaceFitterT<NDIM,NDIM_W,2,CT> SurfaceFitter;
    typedef typename SurfaceFitter::FitFunctor Fit;

    template <class G, typename T1, typename T2, typename T3>
    static void getCoordinates(G *geometry,const T1* ref,const T2* in, T3 *out)
    {
      for (int j=0;j<NDIM_W;++j)
	out[j]=geometry->correctCoordsDiff(in[j]-ref[j],j);
    }

    static void refine(T *result, const M *mesh, 
		       const SEG &seg, const V *newVertex,
     		       S * const *simplices, int nSimplices, void *buffer)
    {  
      /*
      int debug=-1;
      for (int i=0;i<nSimplices;++i) 
	if (simplices[i]->getLocalIndex()==19296) debug=i;
      if (debug>=0) glb::console->print<LOG_STD>("DEBUGGING STARTED\n");
      */

      typename M::GeometricProperties *geometry = mesh->getGeometry();    
      T midPoint[NDIM_W];           
      geometry->midPointCoords(seg->getVertex(0)->getCoordsConstPtr(),
			       seg->getVertex(1)->getCoordsConstPtr(),
			       midPoint);      
    
      int si=simplices[0]->findSegmentIndex(seg);
      std::copy_n(simplices[0]->segTracers.getPointer() + (si*NDIM_W),
		  NDIM_W,result);
         
      // this maps simplex vertices to the new segments departing from them if any
      std::map<V*,int> vertexMap;
      for (int i=0;i<nSimplices;++i)
	{
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V *v=simplices[i]->getVertex(j);
	      if ((v==seg->getVertex(0))||(v==seg->getVertex(1))) continue;
	      
	      if (vertexMap.find(v)==vertexMap.end()) 
		{
		  int index = vertexMap.size();
		  vertexMap.insert(std::make_pair(v,index));
		}
	    }	   
	}

      // newRefSegT are the tracers corresponding to the split segment
      Coord newRefSegT[2][NDIM_W]={{0}};            
      // newSegT are the tracers for the newly created segments
      std::vector<Coord> newSegT(vertexMap.size()*NDIM_W,0);
      // number of contributions from neighbor simplices to compute newSegT tracers position
      std::vector<int> nContribs(vertexMap.size(),0);
     
      // Now compute the new tracers coordinates and put them into the buffer 
      // so that we can use them to refine simplices data.
      // This version computes new fits in the refined simplices base (slower but more
      // accurate as the new base is closer to the tangent space ...)
      for (int i=0;i<nSimplices;++i)
	{	  	 
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  Tracer *tBuffer = static_cast<Tracer*>(curBuffer);
	  
	  // p[k][] contains the vertices of the simplex containing the ith vertex of 
	  // the split segment
	  const Coord *p[2][S::NVERT];
	  for (int j=0;j<S::NVERT;++j)
	    {
	      V *v=simplices[i]->getVertex(j);
	      if (v==seg->getVertex(0))
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=result;	
		}
	      else if (v==seg->getVertex(1))
		{
		  p[1][j]=v->getCoordsPtr();
		  p[0][j]=result;			
		}
	      else
		{
		  p[0][j]=v->getCoordsPtr();
		  p[1][j]=v->getCoordsPtr();	
		}
	    }
	  
	  // n is the index of the split simplex (formed by the p[n][] vertices)
	  for (int n=0;n<2;++n)
	    {
	      SurfaceFitter sf;
	      Coord org[NDIM_W];
	      Coord point[NDIM_W];	      

	      // Origin of the base : the barycenter of the simplex
	      std::fill_n(org,NDIM_W,0);      
	      for (int j=0;j<S::NVERT;++j) 
		{
		  //const Coord *coords=simplices[i]->getVertex(j)->getCoordsConstPtr();
	      
		  for (int k=0;k<NDIM_W;++k)
		    org[k] += geometry->
		      checkCoordConsistency(p[n][j][k],midPoint[k],k);
		}
	      for (int j=0;j<NDIM_W;++j) org[j]/=S::NVERT;

	      // Now add vertices coordinates to fit. 
	      // We will add 13 in 2D and 19 in 3D.
	      // Points are added in order of importance for the fit.
	            
	      // Add the vertices of the (yet unsplit) simplex	      
	      Coord vCoord[S::NVERT][NDIM_W];
	      for (int j=0;j<S::NVERT;++j)
		{
		  getCoordinates(geometry,org,
				 simplices[i]->getVertex(j)->getCoordsPtr(),
				 vCoord[j]);	       
		  sf.addPoint(vCoord[j]);
		}

	      // as well as the segment tracers
	      for (int j=0;j<S::NSEG;++j)
		{
		  getCoordinates(geometry,org,
				 &simplices[i]->segTracers.getPointer()[j*NDIM_W],
				 point);	       
		  sf.addPoint(point);
		}

	      // We now have enough for fitting at order 2, but we prefer solving
	      // an overdetermined problem ...
	      // add the simplex tracer 
	      getCoordinates(geometry,org,simplices[i]->tracer.getPointer(),point);
	      sf.addPoint(point);	

	      // The neighbors simplices tracers
	      for (int j=0;j<S::NNEI;++j)
		{
		  S *nei=simplices[i]->getNeighbor(j);
		  if (nei!=NULL)
		    {		    
		      getCoordinates(geometry,org,
				     nei->tracer.getPointer(),
				     point);
		      sf.addPoint(point);		   
		    }
		}

	      // And the neighbors vertices that do not belong to the current simplex
	      std::set<Coord *> vertices; 
	      Neighborhood nb;
	      simplices[i]->getNeighborhood(nb);
	      for (int j=0;j<S::NNEI;++j)
		{
		  if (nb.neiS[j]!=NULL)
		    vertices.insert(nb.neiV[j]->getCoordsPtr());		    
		}

	      typedef typename std::set<Coord *>::iterator Set_it;
	      for (Set_it it=vertices.begin();it!=vertices.end();++it)
		{
		  getCoordinates(geometry,org,(*it),point);
		  sf.addPoint(point);
		}	 

	      /*
	      getCoordinates(geometry,org,result,point);
	      sf.addPoint(point);

	      std::set<Coord *> vertices; 
	      Neighborhood nb;
	      simplices[i]->getNeighborhood(nb);
	      for (int j=0;j<S::NNEI;++j)
		{
		  if (nb.neiS[j]!=NULL)
		    {
		      vertices.insert(nb.neiV[j]->getCoordsPtr());
		      //vertices.insert(nb.neiS[j]->tracer.getPointer());
		      getCoordinates(geometry,org,
				     nb.neiS[j]->tracer.getPointer(),point);
		      sf.addPoint(point);		   
		    }
		}

	      typedef typename std::set<Coord *>::iterator Set_it;
	      for (Set_it it=vertices.begin();it!=vertices.end();++it)
		{
		  getCoordinates(geometry,org,(*it),point);
		  sf.addPoint(point);
		}	 
	      */

	      // Sets the tangent base from the simplex (first guess) ...
	      Coord base[NDIM][NDIM_W];
	      geometry->template getBaseVectors<Coord,Coord,NDIM,NDIM_W>(p[n],base); 
	      sf.setTangentSpaceBase(base);

	      // And refit the tangent space for a better estimate ?
	      //double cond;
	      Fit fit=sf.fit();	      
	      //sf.refit(fit,vCoord,S::NVERT);
	      
	      Tracer newTCoord[NDIM_W];
	      // Barycenter tracer
	      std::fill_n(point,NDIM_W,0);
	      fit.projectToSurface(point,newTCoord);
	      
	      for (int j=0;j<NDIM_W;++j) 
		tBuffer[n*NDIM_W+j]=newTCoord[j]+org[j];
	      /*
	      if (i==debug)
		{
		  simplices[i]->template print<LOG_STD>("S:");
		  glb::console->print<LOG_STD>("Fitting barycenter%d @(%e %e %e %e) ->(%e %e %e %e)\n",
					       n,org[0],org[1],org[2],org[3],
					       tBuffer[n*NDIM_W+0],tBuffer[n*NDIM_W+1],
					       tBuffer[n*NDIM_W+2],tBuffer[n*NDIM_W+3]);
					       
		}
	      */
	      // Segment tracers
	      Coord p2[NDIM_W];
	      getCoordinates(geometry,org,result,point);      
	      getCoordinates(geometry,org,seg->getVertex(n)->getCoordsConstPtr(),p2);
	      for (int j=0;j<NDIM_W;++j) point[j]=(point[j]+p2[j])*0.5;
	      fit.projectToSurface(point,newTCoord);
	      
	      for (int j=0;j<NDIM_W;++j) newRefSegT[n][j]+=newTCoord[j]+org[j];
	      
	      for (int k=0;k<S::NVERT;++k)
		{
		  V *v=simplices[i]->getVertex(k);
		  if ((seg->getVertex(0)!=v)&&(seg->getVertex(1)!=v))
		    {
		      getCoordinates(geometry,org,result,point);      
		      getCoordinates(geometry,org,v->getCoordsConstPtr(),p2);
		      for (int j=0;j<NDIM_W;++j) point[j]=(point[j]+p2[j])*0.5;
		      fit.projectToSurface(point,newTCoord);
		      int index=(*vertexMap.find(v)).second;
		      nContribs[index]++;
		      for (int j=0;j<NDIM_W;++j) 
			newSegT[index*NDIM_W+j]+=newTCoord[j]+org[j];
		    }
		}	      
	    }

	  // This is used to recognize which tracer belongs to which split simplex	 
	  void *b = &tBuffer[2*NDIM_W+S::NVERT*NDIM_W]; 
	  V** vBuffer = static_cast<V**>(b);
	  vBuffer[0]=seg->getVertex(0);
	  vBuffer[1]=seg->getVertex(1);
	}

      for (int i=0;i<nContribs.size();++i) 
	{
	  for (int j=0;j<NDIM_W;++j) newSegT[NDIM_W*i+j]/=nContribs[i];	    
	  mesh->getGeometry()->checkBoundary(&newSegT[NDIM_W*i]);
	}    
      for (int i=0;i<2;++i)
	{
	  for (int j=0;j<NDIM_W;++j) newRefSegT[i][j]/=nSimplices;	  
	  mesh->getGeometry()->checkBoundary(&newRefSegT[i][0]);
	}

      for (int i=0;i<nSimplices;++i)
	{
	  void *curBuffer = &static_cast<char*>(buffer)[SIMPLEX_BUFFER_SIZE*i];
	  Tracer *tBuffer = &static_cast<Tracer*>(curBuffer)[2*NDIM_W];
	  void *b = &tBuffer[S::NVERT*NDIM_W]; 
	  V** vBuffer = &static_cast<V**>(b)[2];

	  std::copy_n(&newRefSegT[0][0],2*NDIM_W,tBuffer);
	  /*
	    if (i==debug)
	    {
	      glb::console->print<LOG_STD>("PA@: (%e %e %e %e)\n",
					   tBuffer[0],tBuffer[1],tBuffer[2],tBuffer[3]);
	      glb::console->print<LOG_STD>("PB@: (%e %e %e,%e)\n",
					   tBuffer[0+4],tBuffer[1+4],
					   tBuffer[2+4],tBuffer[3+4]);
	    }
	  */
	  tBuffer+=2*NDIM_W;

	  for (int j=0;j<S::NVERT;++j)
	    {
	      V *v=simplices[i]->getVertex(j);
	      if ((seg->getVertex(0)!=v)&&(seg->getVertex(1)!=v))
		{
		  int index=(*vertexMap.find(v)).second;
		  std::copy_n(&newSegT[index*NDIM_W],NDIM_W,tBuffer);
		  (*vBuffer)=v;
		  tBuffer+=NDIM_W;
		  vBuffer++;
		}
	    }
	}
      //if (debug>=0) exit(-1);
      mesh->getGeometry()->checkBoundary(result);
    }
  };
 
}

/** \}*/
#include "../internal/namespace.footer"
#endif
