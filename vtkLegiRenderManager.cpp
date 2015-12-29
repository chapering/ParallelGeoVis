// ----------------------------------------------------------------------------
// vtkLegiRenderManager.cpp : an extension to vtkLegiRenderManager
//	for customizing camera update in all satellite renderers in their corresponding
//	children processes within a global parallel rendering control
//
// Creation : Jan. 19th 2012
//
// Copyright(C) 2011-2012 Haipeng Cai
//
// ----------------------------------------------------------------------------
//
#include "vtkLegiRenderManager.h"

#include "vtkCompressCompositer.h"
#include "vtkFloatArray.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"

#include "vtkCamera.h"
#include "vtkLightCollection.h"
#include "vtkLight.h"
#include "vtkMath.h"
#include "vtkMultiProcessStream.h" // needed for vtkMultiProcessStream.
#include "vtkRendererCollection.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"

vtkStandardNewMacro(vtkLegiRenderManager);

//----------------------------------------------------------------------------
vtkLegiRenderManager::vtkLegiRenderManager()
{
}

//----------------------------------------------------------------------------
vtkLegiRenderManager::~vtkLegiRenderManager()
{
}

//----------------------------------------------------------------------------
void vtkLegiRenderManager::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Composite render manager with camera synchronization." << endl;
}

//----------------------------------------------------------------------------
void vtkLegiRenderManager::PreRenderProcessing()
{
  this->Superclass::PreRenderProcessing();
}

//----------------------------------------------------------------------------
void vtkLegiRenderManager::PostRenderProcessing()
{
  this->Superclass::PostRenderProcessing();
}

void vtkLegiRenderManager::UpdateCamera()
{
  vtkParallelRenderManager::RendererInfo renInfo;
  vtkParallelRenderManager::LightInfo lightInfo;

  vtkDebugMacro("Start updating camera");

  if ((this->Controller == NULL) || (this->Lock))
    {
    return;
    }
  this->Lock = 1;

  if (!this->ParallelRendering)
    {
    this->Lock = 0;
    return;
    }

  // Collect and distribute information about current state of RenderWindow
  vtkRendererCollection *rens = this->GetRenderers();

  // Gather information about the window to send.
  vtkMultiProcessStream stream;

  vtkCollectionSimpleIterator cookie;
  vtkRenderer *ren;
  int i;

  for (rens->InitTraversal(cookie), i = 0;
       (ren = rens->GetNextRenderer(cookie)) != NULL; i++)
    {
    ren->GetViewport(renInfo.Viewport);

    int hasActiveCamera=ren->IsActiveCameraCreated();
    vtkCamera *cam = ren->GetActiveCamera();
    if(!hasActiveCamera)
      {
      this->ResetCamera(ren);
      }
    cam->GetPosition(renInfo.CameraPosition);
    cam->GetFocalPoint(renInfo.CameraFocalPoint);
    cam->GetViewUp(renInfo.CameraViewUp);
    cam->GetClippingRange(renInfo.CameraClippingRange);
    renInfo.CameraViewAngle = cam->GetViewAngle();
    cam->GetWindowCenter(renInfo.WindowCenter);
        
    ren->GetBackground(renInfo.Background);
    ren->GetBackground2(renInfo.Background2);
    renInfo.GradientBackground=ren->GetGradientBackground();
    if (cam->GetParallelProjection())
      {
      renInfo.ParallelScale = cam->GetParallelScale();
      }
    else
      {
      renInfo.ParallelScale = 0.0;
      }
    renInfo.Draw = ren->GetDraw();
    vtkLightCollection *lc = ren->GetLights();
    renInfo.NumberOfLights = lc->GetNumberOfItems();
    renInfo.Save(stream);

    vtkLight *light;
    vtkCollectionSimpleIterator lsit;
    for (lc->InitTraversal(lsit); (light = lc->GetNextLight(lsit)); )
      {
      lightInfo.Type = (double)(light->GetLightType());
      light->GetPosition(lightInfo.Position);
      light->GetFocalPoint(lightInfo.FocalPoint);
      lightInfo.Save(stream); 
      }
    this->CollectRendererInformation(ren, stream);
    }

  /* send the camera info associated with root process to all children processes */
  if (!this->Controller->Broadcast(stream, this->Controller->GetLocalProcessId()))
    {
    return;
    }

  // Backwards compatibility stuff.
  this->SendWindowInformation();
  rens->InitTraversal(cookie);
  while ((ren = rens->GetNextRenderer(cookie)) != NULL)
    {
    this->SendRendererInformation(ren);
    }
}

void vtkLegiRenderManager::SatelliteUpdateCamera()
{
  vtkParallelRenderManager::RendererInfo renInfo;
  vtkParallelRenderManager::LightInfo lightInfo;
  int i, j;

  vtkDebugMacro("SatelliteUpdateCamera");

  vtkMultiProcessStream stream;
  /* receive the latest renderer info broadcast from the root process */
  if (!this->Controller->Broadcast(stream, this->RootProcessId))
    {
    return;
    }

  vtkCollectionSimpleIterator rsit;
  vtkRendererCollection *rens = this->GetRenderers();
  vtkRenderer *ren;

  for (rens->InitTraversal(rsit), i = 0;
       (ren = rens->GetNextRenderer(rsit)) != NULL; i++)
    {
    vtkLightCollection *lc = NULL;
    vtkCollectionSimpleIterator lsit;
    vtkRenderer *ren = rens->GetNextRenderer(rsit);
    if (ren == NULL)
      {
      vtkErrorMacro("Not enough renderers");
      }
    else
      {
      // Backwards compatibility
      this->ReceiveRendererInformation(ren);

      if (!renInfo.Restore(stream))
        {
        vtkErrorMacro("Failed to read renderer information for " << i);
        continue;
        }

      ren->SetViewport(renInfo.Viewport);
      ren->SetBackground(renInfo.Background[0],
                         renInfo.Background[1],
                         renInfo.Background[2]);
      ren->SetBackground2(renInfo.Background2[0],
                          renInfo.Background2[1],
                          renInfo.Background2[2]);
      ren->SetGradientBackground(renInfo.GradientBackground);
      vtkCamera *cam = ren->GetActiveCamera();
      cam->SetPosition(renInfo.CameraPosition);
      cam->SetFocalPoint(renInfo.CameraFocalPoint);
      cam->SetViewUp(renInfo.CameraViewUp);
      cam->SetClippingRange(renInfo.CameraClippingRange);
      cam->SetViewAngle(renInfo.CameraViewAngle);
      cam->SetWindowCenter(renInfo.WindowCenter[0],
                           renInfo.WindowCenter[1]);
      if (renInfo.ParallelScale != 0.0)
        {
        cam->ParallelProjectionOn();
        cam->SetParallelScale(renInfo.ParallelScale);
        }
      else
        {
        cam->ParallelProjectionOff();
        }
      ren->SetDraw(renInfo.Draw);
      lc = ren->GetLights();
      lc->InitTraversal(lsit);
      }

    for (j = 0; j < renInfo.NumberOfLights; j++)
      {
      if (ren != NULL && lc != NULL)
        {
        vtkLight *light = lc->GetNextLight(lsit);
        if (light == NULL)
          {
          // Not enough lights?  Just create them.
          vtkDebugMacro("Adding light");
          light = vtkLight::New();
          ren->AddLight(light);
          light->Delete();
          }

        if (!lightInfo.Restore(stream))
          {
          vtkErrorMacro("Failed to read light information");
          continue;
          }
        light->SetLightType((int)(lightInfo.Type));
        light->SetPosition(lightInfo.Position);
        light->SetFocalPoint(lightInfo.FocalPoint);
        }
      }

    if (ren != NULL)
      {
      vtkLight *light;
      while ((light = lc->GetNextLight(lsit)))
        {
        // To many lights?  Just remove the extras.
        ren->RemoveLight(light);
        }
      }

    if (!this->ProcessRendererInformation(ren, stream))
      {
      vtkErrorMacro("Failed to process renderer information correctly.");
      }
    }
}

//----------------------------------------------------------------------------
void vtkLegiRenderManager::GenericUpdateCamera()
{
  if (!this->Controller)
    {
    return;
    }

  if (this->Controller->GetLocalProcessId() == this->RootProcessId)
    {
    this->UpdateCamera();
    }
  else // LocalProcessId != RootProcessId
    {
    this->SatelliteUpdateCamera();
    }
}

