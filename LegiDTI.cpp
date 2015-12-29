#include "QVTKApplication.h"
#include "vmRenderWindow.h"

#include "vtkExecutive.h"

#include "vtkTreeCompositer.h"
#include "vtkCompressCompositer.h"

//#include "kaapi++"

typedef struct _cmdpara {
	int argc;
	char** argv;
}cmdpara;

//vtkSmartPointer<vtkCamera> g_cam;

/* extend RMI messages */
#define RENDER_RMI_CAMERA_UPDATE 87840
#define RENDER_RMI_MAPPING_UPDATE 87841
#define RENDER_RMI_REDRAW 87842

static bool g_bShowAllSlaves = false;

static void UpdateCamera(void *arg, void *, int, int)
{
  //vtkParallelRenderManager *self = (vtkParallelRenderManager *)arg;
  //self->RenderRMI();
  vtkLegiRenderManager* self = (vtkLegiRenderManager*)arg;
  cout << "local pid: " << self->GetController()->GetLocalProcessId() << "\n";
  self->GenericUpdateCamera();
}

void UpdateMapping(void *arg, void *, int, int)
{
  CLegiMainWindow* self = (CLegiMainWindow*)arg;

  vtkPolyDataAlgorithm* agent = self->m_dsort;
  vtkPolyDataAlgorithm* agent2 = self->m_linesort;
  vtkPolyDataAlgorithm* streamtube = self->m_streamtube;
  vtkActor*	actor = self->m_actor;

  bool updateActor = false;

  if ( agent2 != NULL && agent2->GetInput() != NULL && vtkTubeFilter::SafeDownCast(streamtube)->GetVaryRadius() > 0) 
  {
	  agent2->Update();
	  streamtube->GetInput()->Modified();
  }

  if ( agent != NULL && agent->GetTotalNumberOfInputConnections() >= 1) 
  {
	  agent->Update();
	  updateActor = true;
  }

  //self->m_controller->TriggerRMIOnAllChildren(vtkMultiProcessController::BREAK_RMI_TAG);
  //self->m_controller->TriggerRMIOnAllChildren(vtkParallelRenderManager::RENDER_RMI_TAG);

  if ( updateActor && actor ) {
	  //actor->GetMapper()->ReleaseGraphicsResources( self->renderView->GetRenderWindow() );
	  //actor->GetMapper()->GetInput()->Modified();
	  //vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->GetOutput()->Modified();
	  vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->GetInput()->Modified();
	  //actor->GetMapper()->ImmediateModeRenderingOn();
	  //vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->Draw( self->m_render, actor );

	  //actor->GetMapper()->Update();
	  //cout << "mapper updated.\n";
	  //actor->GetMapper()->GetInput()->InvokeEvent(vtkCommand::ModifiedEvent,NULL);

  }
}

void Redraw(void *arg, void *, int, int)
{
  CLegiMainWindow* self = (CLegiMainWindow*)arg;

  cout << "button apply pressed.\n";
  self->onButtonApply();
}

void process(vtkMultiProcessController* controller, void* arg)
{
	int myId = controller->GetLocalProcessId();

	CLegiMainWindow win(controller);
	win.setWindowTitle( "LegiDTI 1.0.0" );
	//CLegiMainWindow* wm = (CLegiMainWindow*)arg;
	CLegiMainWindow* wm = &win;

	//vtkRenderWindow* renWin = vtkRenderWindow::New();

	vtkLegiRenderManager* tc = vtkLegiRenderManager::New();
	tc->SetRenderWindow(wm->renderView->GetRenderWindow());
	//tc->SetRenderWindow(renWin);
	//
	vtkTreeCompositer * cs = vtkTreeCompositer::New();
	//vtkCompressCompositer* cs = vtkCompressCompositer::New();
	cs->Register( tc );
	tc->SetCompositer( cs );
	cs->Delete();
	
	unsigned long cameraRMIid = controller->AddRMI( ::UpdateCamera, tc, RENDER_RMI_CAMERA_UPDATE );
	unsigned long mappingRMIid = controller->AddRMI( ::UpdateMapping, wm, RENDER_RMI_MAPPING_UPDATE );
	unsigned long rdRMIid = controller->AddRMI( ::Redraw, wm, RENDER_RMI_REDRAW );
	
	//controller->Barrier();
	//controller->GetCommunicator()->BroadcastVoidArray((void*)&g_cam, 1, VTK_UNSIGNED___INT64, 0);

	//wm->m_camera = g_cam;

	wm->show();

	/*
	renWin->AddRenderer( wm->m_render );

	if ( myId == 0 ) {
		vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::New();
		iren->SetRenderWindow(renWin);
		tc->ResetAllCameras ();
	}
	else {
		//vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::New();
		//iren->SetRenderWindow(wm->renderView->GetRenderWindow());
		//iren->SetRenderWindow(renWin);
	}
	*/

	if ( !g_bShowAllSlaves ) {
		tc->InitializeOffScreen();
	}

	//tc->InitializePieces();
	tc->SetMagnifyImageMethodToNearest();
	tc->AutoImageReductionFactorOn();
	/*
	tc->SetImageReductionFactor(5.0);
	tc->MagnifyImagesOn();
	tc->SetForcedRenderWindowSize(5000,1000);
	*/

	tc->SetUseRGBA(1);
	tc->SetForcedRenderWindowSize(100,100);

	/*
	tc->AlphaBitPlanesOn();
	tc->PointSmoothingOn ();
	tc->LineSmoothingOn();
	tc->PolygonSmoothingOn();
	*/

	controller->Barrier();

	wm->renderView->GetRenderWindow()->LineSmoothingOn();
	wm->renderView->GetRenderWindow()->AlphaBitPlanesOn ();
	if ( myId == 0 ) {
		tc->WriteBackImagesOn();
		tc->ResetAllCameras ();
		wm->m_render->GetActiveCamera()->Zoom(2.0);
		//tc->WriteBackImagesOff();
		//

		qApp->exec();
		tc->StopServices();
	}
	else {
		/*
		wm->renderView->GetRenderWindow()->HideCursor();
		wm->renderView->GetRenderWindow()->BordersOff();
		wm->renderView->GetRenderWindow()->SetPosition(0,0);
		wm->renderView->GetRenderWindow()->SetSize(10,10);
		wm->renderView->GetRenderWindow()->OffScreenRenderingOn();
		wm->renderView->GetRenderWindow()->DoubleBufferOn();
		wm->renderView->GetRenderWindow()->EraseOn();
		*/
		wm->renderView->GetRenderWindow()->DoubleBufferOn();

		if ( !g_bShowAllSlaves ) {
			wm->hide();
		}
		tc->UseBackBufferOff();
		tc->WriteBackImagesOff();
		tc->StartInteractor();
		//tc->StartServices();
	}

	controller->RemoveRMI( cameraRMIid );
	controller->RemoveRMI( mappingRMIid );
	controller->RemoveRMI( rdRMIid );

	tc->Delete();
}

int main(int argc, char* argv[])
{
	//ka::Community com = ka::System::join_community(qApp->argc(), qApp->argv());
	/*
	ka::Community com = ka::System::join_community(argc, argv);
	com.commit();
	*/

	vtkMPIController* controller = vtkMPIController::New();

	controller->Initialize(&argc, &argv);
	vtkMultiProcessController::SetGlobalController(controller);

	QVTKApplication app(argc, argv);

	if ( argc >= 2 && strncmp(argv[1], "-c", 2) == 0 ) {
		g_bShowAllSlaves = true;
	}

	//win.selfRotate(5);
	//win.show();

	if (controller->IsA("vtkThreadedController"))
	{
		controller->SetNumberOfProcesses(4);
	}

	if ( controller->GetNumberOfProcesses() < 2 )
	{
		cerr << "This program is designed for parallel rendering thus requires more than one processor." << endl;
		return 1;
	}

	/*
	g_cam = vtkSmartPointer<vtkCamera>::New();
	g_cam->SetPosition( 0,0,1 );
	g_cam->SetFocalPoint( 0, 0, 0 );
	g_cam->SetViewUp( 0,1,0 );

	g_cam->SetClippingRange( 10, 110.67);
	g_cam->Zoom(2.0);

	g_cam->Delete();
	*/

	controller->SetSingleMethod(process, 0);
	controller->SingleMethodExecute();

	controller->Finalize();
	vtkMultiProcessController::SetGlobalController(NULL);
	controller->Delete();

	/*
	com.leave();
	ka::System::terminate();
	*/

	return 0;
}

