// ----------------------------------------------------------------------------
// vtkTubeFilterEx.cpp : extension to vtkTubeFilter for supporting depth-dependent
//						streamtube geometric properties
//
// Creation : Nov. 22nd 2011
//
// Copyright(C) 2011-2012 Haipeng Cai
//
// ----------------------------------------------------------------------------
#include "vtkTubeFilterEx.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkFloatArray.h"
#include "vtkMath.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyLine.h"

#include "vtkGenericCell.h"
#include "vtkProp3D.h"
#include "vtkTransform.h"
#include "vtkCamera.h"
#include "vtkUnsignedIntArray.h"

#include <iostream>

using namespace std;

vtkStandardNewMacro(vtkTubeFilterEx);

vtkCxxSetObjectMacro(vtkTubeFilterEx,Camera,vtkCamera);

// Construct object with radius 0.5, radius variation turned off, the number 
// of sides set to 3, and radius factor of 10.
vtkTubeFilterEx::vtkTubeFilterEx()
{
	//cout << "vtkTubeFilterEx instantiated.\n";
  this->Camera = NULL;
  this->Prop3D = NULL;
  this->Direction = VTK_DIRECTION_BACK_TO_FRONT;
  this->DepthSortMode = VTK_SORT_FIRST_POINT;
  this->Vector[0] = this->Vector[1] = 0.0;
  this->Vector[2] = 0.0;
  this->Origin[0] = this->Origin[1] = this->Origin[2] = 0.0;
  this->Transform = vtkTransform::New();
  this->SortScalars = 0;
}

vtkTubeFilterEx::~vtkTubeFilterEx()
{
  this->Transform->Delete();
  
  if ( this->Camera )
    {
    this->Camera->Delete();
    }
}

// Don't reference count to avoid nasty cycle
void vtkTubeFilterEx::SetProp3D(vtkProp3D *prop3d)
{
  if ( this->Prop3D != prop3d )
    {
    this->Prop3D = prop3d;
    this->Modified();
    }
}

vtkProp3D *vtkTubeFilterEx::GetProp3D()
{
  return this->Prop3D;
}

typedef struct _vtkSortValues {
  double z;
  vtkIdType ptId;
} vtkSortValues;

extern "C" 
{
  int vtkCompareBackToFrontEx(const void *val1, const void *val2)
  {
    if (((vtkSortValues *)val1)->z > ((vtkSortValues *)val2)->z)
      {
      return (-1);
      }
    else if (((vtkSortValues *)val1)->z < ((vtkSortValues *)val2)->z)
      {
      return (1);
      }
    else
      {
      return (0);
      }
  }
}

extern "C" 
{
  int vtkCompareFrontToBackEx(const void *val1, const void *val2)
  {
    if (((vtkSortValues *)val1)->z < ((vtkSortValues *)val2)->z)
      {
      return (-1);
      }
    else if (((vtkSortValues *)val1)->z > ((vtkSortValues *)val2)->z)
      {
      return (1);
      }
    else
      {
      return (0);
      }
  }
}

int vtkTubeFilterEx::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // get the input and output
  vtkPolyData *input = vtkPolyData::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *output = vtkPolyData::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkPointData *pd=input->GetPointData();
  vtkPointData *outPD=output->GetPointData();
  vtkCellData *cd=input->GetCellData();
  vtkCellData *outCD=output->GetCellData();
  vtkCellArray *inLines;
  vtkDataArray *inNormals;
  vtkDataArray *inScalars=this->GetInputArrayToProcess(0,inputVector);
  vtkDataArray *inVectors=this->GetInputArrayToProcess(1,inputVector);

  vtkPoints *inPts;
  vtkIdType numPts;
  vtkIdType numLines;
  vtkIdType numNewPts, numNewCells;
  vtkPoints *newPts;
  int deleteNormals=0;
  vtkFloatArray *newNormals;
  vtkIdType i;
  double range[2], maxSpeed=0;
  vtkCellArray *newStrips;
  vtkIdType npts=0, *pts=NULL;
  vtkIdType offset=0;
  vtkFloatArray *newTCoords=NULL;
  int abort=0;
  vtkIdType inCellId;
  double oldRadius=1.0;

  // Check input and initialize
  //
  vtkDebugMacro(<<"Creating tube");

  if ( !(inPts=input->GetPoints()) || 
      (numPts = inPts->GetNumberOfPoints()) < 1 ||
      !(inLines = input->GetLines()) || 
       (numLines = inLines->GetNumberOfCells()) < 1 )
    {
    return 1;
    }

  // Create the geometry and topology
  numNewPts = numPts * this->NumberOfSides;
  newPts = vtkPoints::New();
  newPts->Allocate(numNewPts);
  newNormals = vtkFloatArray::New();
  newNormals->SetName("TubeNormals");
  newNormals->SetNumberOfComponents(3);
  newNormals->Allocate(3*numNewPts);
  newStrips = vtkCellArray::New();
  newStrips->Allocate(newStrips->EstimateSize(1,numNewPts));
  vtkCellArray *singlePolyline = vtkCellArray::New();

  // Point data: copy scalars, vectors, tcoords. Normals may be computed here.
  outPD->CopyNormalsOff();
  if ( (this->GenerateTCoords == VTK_TCOORDS_FROM_SCALARS && inScalars) ||
       this->GenerateTCoords == VTK_TCOORDS_FROM_LENGTH ||
       this->GenerateTCoords == VTK_TCOORDS_FROM_NORMALIZED_LENGTH )
    {
    newTCoords = vtkFloatArray::New();
    newTCoords->SetNumberOfComponents(2);
    newTCoords->Allocate(numNewPts);
    outPD->CopyTCoordsOff();
    }
  outPD->CopyAllocate(pd,numNewPts);

  int generateNormals = 0;
  if ( !(inNormals=pd->GetNormals()) || this->UseDefaultNormal )
    {
    deleteNormals = 1;
    inNormals = vtkFloatArray::New();
    inNormals->SetNumberOfComponents(3);
    inNormals->SetNumberOfTuples(numPts);

    if ( this->UseDefaultNormal )
      {
      for ( i=0; i < numPts; i++)
        {
        inNormals->SetTuple(i,this->DefaultNormal);
        }
      }
    else
      {
      // Normal generation has been moved to lower in the function.
      // This allows each different polylines to share vertices, but have
      // their normals (and hence their tubes) calculated independently
      generateNormals = 1;
      }      
    }

  // If varying width, get appropriate info.
  //
  if ( inScalars )
    {
		//cout << "have scalars:\n";
		//inScalars->PrintSelf( cout, vtkIndent() );

    inScalars->GetRange(range,0);
	//cout << range[1] << ", " << range[0] << "\n";
    if ((range[1] - range[0]) == 0.0)
      {
      if (this->VaryRadius == VTK_VARY_RADIUS_BY_SCALAR )
        {
        vtkWarningMacro(<< "Scalar range is zero!");
        }
      range[1] = range[0] + 1.0;
      }
    if (this->VaryRadius == VTK_VARY_RADIUS_BY_ABSOLUTE_SCALAR)
      {
      // temporarily set the radius to 1.0 so that radius*scalar = scalar
      oldRadius = this->Radius;
      this->Radius = 1.0;
      if (range[0] < 0.0)
        {
        vtkWarningMacro(<< "Scalar values fall below zero when using absolute radius values!");
        }
      }
    }
  else {
		//cout << "have no scalars:\n";
  }

  if ( inVectors )
    {
    maxSpeed = inVectors->GetMaxNorm();
    }

  // Copy selected parts of cell data; certainly don't want normals
  //
  numNewCells = inLines->GetNumberOfCells() * this->NumberOfSides + 2;
  outCD->CopyNormalsOff();
  outCD->CopyAllocate(cd,numNewCells);

  //  Create points along each polyline that are connected into NumberOfSides
  //  triangle strips. Texture coordinates are optionally generated.
  //
  this->Theta = 2.0*vtkMath::Pi() / this->NumberOfSides;
  vtkPolyLine *lineNormalGenerator = vtkPolyLine::New();
  for (inCellId=0, inLines->InitTraversal(); 
       inLines->GetNextCell(npts,pts) && !abort; inCellId++)
    {
    this->UpdateProgress((double)inCellId/numLines);
    abort = this->GetAbortExecute();

    if (npts < 2)
      {
      vtkWarningMacro(<< "Less than two points in line!");
      continue; //skip tubing this polyline
      }

    // If necessary calculate normals, each polyline calculates its
    // normals independently, avoiding conflicts at shared vertices.
    if (generateNormals) 
      {
      singlePolyline->Reset(); //avoid instantiation
      singlePolyline->InsertNextCell(npts,pts);
      if ( !lineNormalGenerator->GenerateSlidingNormals(inPts,singlePolyline,
                                                        inNormals) )
        {
        vtkWarningMacro("Could not generate normals for line. "
                        "Skipping to next.");
        continue; //skip tubing this polyline
        }
      }

    // Generate the points around the polyline. The tube is not stripped
    // if the polyline is bad.
    //
    if ( !this->GeneratePoints(offset,npts,pts,inPts,newPts,pd,outPD,
                               newNormals,inScalars,range,inVectors,
                               maxSpeed,inNormals) )
      {
      vtkWarningMacro(<< "Could not generate points!");
      continue; //skip tubing this polyline
      }
      
    // Generate the strips for this polyline (including caps)
    //
    this->GenerateStrips(offset,npts,pts,inCellId,cd,outCD,newStrips);

    // Generate the texture coordinates for this polyline
    //
    if ( newTCoords )
      {
      this->GenerateTextureCoords(offset,npts,pts,inPts,inScalars,newTCoords);
      }

    // Compute the new offset for the next polyline
    offset = this->ComputeOffset(offset,npts);

    }//for all polylines

  singlePolyline->Delete();
  
  // reset the radius to ite orginal value if necessary
  if (this->VaryRadius == VTK_VARY_RADIUS_BY_ABSOLUTE_SCALAR)
    {
    this->Radius = oldRadius;
    }

  // Update ourselves
  //
  if ( deleteNormals )
    {
    inNormals->Delete();
    }

  if ( newTCoords )
    {
    outPD->SetTCoords(newTCoords);
    newTCoords->Delete();
    }

  output->SetPoints(newPts);
  newPts->Delete();

  output->SetStrips(newStrips);
  newStrips->Delete();

  outPD->SetNormals(newNormals);
  newNormals->Delete();
  lineNormalGenerator->Delete();

  output->Squeeze();

  if ( !this->SortScalars ) {
	  return 1;
  }

  /* ---------------------------------------------- */
  // add scalars giving depth sorting information
  static vtkSortValues *depth = NULL;
  static vtkUnsignedIntArray *sortScalars = NULL;
  static unsigned int *scalars = NULL;
  double x[3];
  double *w = NULL;
  double vector[3];
  double origin[3];

  vtkIdType numPoints = output->GetNumberOfPoints();
  vtkIdType ptId;

  // Initialize
  //
  vtkDebugMacro(<<"Sorting polygonal data");

  // Compute the sort vector
  if ( this->Direction == VTK_DIRECTION_SPECIFIED_VECTOR )
    {
    for (int i=0; i<3; i++)
      {
      vector[i] = this->Vector[i];
      origin[i] = this->Origin[i];
      }
    }
  else //compute view vector
    {
    if ( this->Camera == NULL)
      {
      vtkErrorMacro(<<"Need a camera to sort");
      return 0;
      }
  
    this->ComputeProjectionVector(vector, origin);
    }

  if ( this->DepthSortMode == VTK_SORT_PARAMETRIC_CENTER )
    {
    w = new double [output->GetMaxCellSize()];
    }

  // Compute the depth value per points, so this is done regardless of the DepthSortMode
  if ( depth == NULL ) {
	  depth = new vtkSortValues [numPoints];
  }

  for ( ptId=0; ptId < numPoints; ptId++ )
  {
	  output->GetPoint(ptId, x);
	  depth[ptId].z = vtkMath::Dot(x,vector);
	  depth[ptId].ptId= ptId;
  }

  //this->UpdateProgress(0.20);

  // Sort the depths
  if ( this->Direction == VTK_DIRECTION_FRONT_TO_BACK )
    {
    qsort((void *)depth, numPoints, sizeof(vtkSortValues), 
          vtkCompareFrontToBackEx);
    }
  else
    {
    qsort((void *)depth, numPoints, sizeof(vtkSortValues), 
          vtkCompareBackToFrontEx);
    }
  //this->UpdateProgress(0.60);

  // Generate sorted output
  if ( this->SortScalars )
    {
		if ( sortScalars == NULL ) {
			sortScalars = vtkUnsignedIntArray::New();
			sortScalars->SetNumberOfTuples(numPoints);
			scalars = sortScalars->GetPointer(0);
		}

		for ( ptId=0; ptId < numPoints; ptId++ ) {
			scalars[ depth[ptId].ptId ] = ptId;
		}

		output->GetPointData()->SetScalars( sortScalars );
    }
  /* ---------------------------------------------- */
  return 1;
}

void vtkTubeFilterEx::ComputeProjectionVector(double vector[3], 
                                                   double origin[3])
{
  double *focalPoint = this->Camera->GetFocalPoint();
  double *position = this->Camera->GetPosition();
 
  // If a camera is present, use it
  if ( !this->Prop3D )
    {
    for(int i=0; i<3; i++)
      { 
      vector[i] = focalPoint[i] - position[i];
      origin[i] = position[i];
      }
    }

  else  //Otherwise, use Prop3D
    {
    double focalPt[4], pos[4];
    int i;

    this->Transform->SetMatrix(this->Prop3D->GetMatrix());
    this->Transform->Push();
    this->Transform->Inverse();

    for(i=0; i<4; i++)
      {
      focalPt[i] = focalPoint[i];
      pos[i] = position[i];
      }

    this->Transform->TransformPoint(focalPt,focalPt);
    this->Transform->TransformPoint(pos,pos);

    for (i=0; i<3; i++) 
      {
      vector[i] = focalPt[i] - pos[i];
      origin[i] = pos[i];
      }
    this->Transform->Pop();
  }
}

int vtkTubeFilterEx::GeneratePoints(vtkIdType offset, 
                                  vtkIdType npts, vtkIdType *pts,
                                  vtkPoints *inPts, vtkPoints *newPts, 
                                  vtkPointData *pd, vtkPointData *outPD,
                                  vtkFloatArray *newNormals,
                                  vtkDataArray *inScalars, double range[2],
                                  vtkDataArray *inVectors, double maxSpeed,
                                  vtkDataArray *inNormals)
{
  vtkIdType j;
  int i, k;
  double p[3];
  double pNext[3];
  double sNext[3] = {0.0, 0.0, 0.0};
  double sPrev[3];
  double startCapNorm[3], endCapNorm[3];
  double n[3];
  double s[3];
  //double bevelAngle;
  double w[3];
  double nP[3];
  double sFactor=1.0;
  double normal[3];
  vtkIdType ptId=offset;

  // Use "averaged" segment to create beveled effect. 
  // Watch out for first and last points.
  //
  for (j=0; j < npts; j++)
    {
    if ( j == 0 ) //first point
      {
      inPts->GetPoint(pts[0],p);
      inPts->GetPoint(pts[1],pNext);
      for (i=0; i<3; i++) 
        {
        sNext[i] = pNext[i] - p[i];
        sPrev[i] = sNext[i];
        startCapNorm[i] = -sPrev[i];
        }
      vtkMath::Normalize(startCapNorm);
      }
    else if ( j == (npts-1) ) //last point
      {
      for (i=0; i<3; i++)
        {
        sPrev[i] = sNext[i];
        p[i] = pNext[i];
        endCapNorm[i] = sNext[i];
        }
      vtkMath::Normalize(endCapNorm);
      }
    else
      {
      for (i=0; i<3; i++)
        {
        p[i] = pNext[i];
        }
      inPts->GetPoint(pts[j+1],pNext);
      for (i=0; i<3; i++)
        {
        sPrev[i] = sNext[i];
        sNext[i] = pNext[i] - p[i];
        }
      }

    inNormals->GetTuple(pts[j], n);

    if ( vtkMath::Normalize(sNext) == 0.0 )
      {
      vtkWarningMacro(<<"Coincident points!");
      return 0;
      }

    for (i=0; i<3; i++)
      {
      s[i] = (sPrev[i] + sNext[i]) / 2.0; //average vector
      }
    // if s is zero then just use sPrev cross n
    if (vtkMath::Normalize(s) == 0.0)
      {
      vtkDebugMacro(<< "Using alternate bevel vector");
      vtkMath::Cross(sPrev,n,s);
      if (vtkMath::Normalize(s) == 0.0)
        {
        vtkDebugMacro(<< "Using alternate bevel vector");
        }
      }

/*    if ( (bevelAngle = vtkMath::Dot(sNext,sPrev)) > 1.0 )
      {
      bevelAngle = 1.0;
      }
    if ( bevelAngle < -1.0 )
      {
      bevelAngle = -1.0;
      }
    bevelAngle = acos((double)bevelAngle) / 2.0; //(0->90 degrees)
    if ( (bevelAngle = cos(bevelAngle)) == 0.0 )
      {
      bevelAngle = 1.0;
      }

    bevelAngle = this->Radius / bevelAngle; //keep tube constant radius
*/
    vtkMath::Cross(s,n,w);
    if ( vtkMath::Normalize(w) == 0.0)
      {
      vtkWarningMacro(<<"Bad normal s = " <<s[0]<<" "<<s[1]<<" "<< s[2] 
                      << " n = " << n[0] << " " << n[1] << " " << n[2]);
      return 0;
      }

    vtkMath::Cross(w,s,nP); //create orthogonal coordinate system
    vtkMath::Normalize(nP);

    // Compute a scale factor based on scalars or vectors
    if ( inScalars && this->VaryRadius == VTK_VARY_RADIUS_BY_SCALAR )
      {
      sFactor = 1.0 + ((this->RadiusFactor - 1.0) * 
                (inScalars->GetComponent(pts[j],0) - range[0]) 
                       / (range[1]-range[0]));
      }
    else if ( inVectors && this->VaryRadius == VTK_VARY_RADIUS_BY_VECTOR )
      {
      sFactor = 
        sqrt((double)maxSpeed/vtkMath::Norm(inVectors->GetTuple(pts[j])));
      if ( sFactor > this->RadiusFactor )
        {
        sFactor = this->RadiusFactor;
        }
      }
    else if ( inScalars && 
              this->VaryRadius == VTK_VARY_RADIUS_BY_ABSOLUTE_SCALAR )
      {
      sFactor = inScalars->GetComponent(pts[j],0);
      if (sFactor < 0.0) 
        {
        vtkWarningMacro(<<"Scalar value less than zero, skipping line");
        return 0;
        }
      }

    //create points around line
    if (this->SidesShareVertices)
      {
      for (k=0; k < this->NumberOfSides; k++)
        {
        for (i=0; i<3; i++) 
          {
          normal[i] = w[i]*cos((double)k*this->Theta) + 
            nP[i]*sin((double)k*this->Theta);
          s[i] = p[i] + this->Radius * sFactor * normal[i];
          }
        newPts->InsertPoint(ptId,s);
        newNormals->InsertTuple(ptId,normal);
        outPD->CopyData(pd,pts[j],ptId);
        ptId++;
        }//for each side
      } 
    else
      {
      double n_left[3], n_right[3];
      for (k=0; k < this->NumberOfSides; k++)
        {
        for (i=0; i<3; i++)
          {
          // Create duplicate vertices at each point
          // and adjust the associated normals so that they are
          // oriented with the facets. This preserves the tube's
          // polygonal appearance, as if by flat-shading around the tube,
          // while still allowing smooth (gouraud) shading along the
          // tube as it bends.
          normal[i]  = w[i]*cos((double)(k+0.0)*this->Theta) + 
            nP[i]*sin((double)(k+0.0)*this->Theta);
          n_right[i] = w[i]*cos((double)(k-0.5)*this->Theta) + 
            nP[i]*sin((double)(k-0.5)*this->Theta);
          n_left[i]  = w[i]*cos((double)(k+0.5)*this->Theta) + 
            nP[i]*sin((double)(k+0.5)*this->Theta);
          s[i] = p[i] + this->Radius * sFactor * normal[i];
          }
        newPts->InsertPoint(ptId,s);
        newNormals->InsertTuple(ptId,n_right);
        outPD->CopyData(pd,pts[j],ptId);
        newPts->InsertPoint(ptId+1,s);
        newNormals->InsertTuple(ptId+1,n_left);
        outPD->CopyData(pd,pts[j],ptId+1);
        ptId += 2;
        }//for each side
      }//else separate vertices
    }//for all points in polyline
  
  //Produce end points for cap. They are placed at tail end of points.
  if (this->Capping)
    {
    int numCapSides = this->NumberOfSides;
    int capIncr = 1;
    if ( ! this->SidesShareVertices )
      {
      numCapSides = 2 * this->NumberOfSides;
      capIncr = 2;
      }

    //the start cap
    for (k=0; k < numCapSides; k+=capIncr)
      {
      newPts->GetPoint(offset+k,s);
      newPts->InsertPoint(ptId,s);
      newNormals->InsertTuple(ptId,startCapNorm);
      outPD->CopyData(pd,pts[0],ptId);
      ptId++;
      }
    //the end cap
    int endOffset = offset + (npts-1)*this->NumberOfSides;
    if ( ! this->SidesShareVertices )
      {
      endOffset = offset + 2*(npts-1)*this->NumberOfSides;      
      }
    for (k=0; k < numCapSides; k+=capIncr)
      {
      newPts->GetPoint(endOffset+k,s);
      newPts->InsertPoint(ptId,s);
      newNormals->InsertTuple(ptId,endCapNorm);
      outPD->CopyData(pd,pts[npts-1],ptId);
      ptId++;
      }
    }//if capping
  
  return 1;
}

unsigned long int vtkTubeFilterEx::GetMTime()
{
  unsigned long mTime=this->Superclass::GetMTime();
 
  if ( this->Direction != VTK_DIRECTION_SPECIFIED_VECTOR )
    {
    unsigned long time;
    if ( this->Camera != NULL )
      {
      time = this->Camera->GetMTime();
      mTime = ( time > mTime ? time : mTime );
      }

    if ( this->Prop3D != NULL )
      {
      time = this->Prop3D->GetMTime();
      mTime = ( time > mTime ? time : mTime );
      }
    }

  return mTime;
}

