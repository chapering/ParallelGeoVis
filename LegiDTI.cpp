#include "QVTKApplication.h"
#include "vmRenderWindow.h"

//#include "kaapi++"

typedef struct _cmdpara {
	int argc;
	char** argv;
}cmdpara;

//vtkSmartPointer<vtkCamera> g_cam;

/* extend RMI messages */
#define RENDER_RMI_CAMERA_UPDATE 87840
#define RENDER_RMI_MAPPING_UPDATE 87841

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

  if ( updateActor && actor ) {
	  actor->GetMapper()->GetInput()->Modified();
	  //cout << "mapper updated.\n";
  }
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
	
	/*
	controller->AddRMI( ::UpdateCamera, tc, RENDER_RMI_CAMERA_UPDATE );
	controller->AddRMI( ::UpdateMapping, wm, RENDER_RMI_MAPPING_UPDATE );
	*/
	
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

	tc->InitializeOffScreen();
	//tc->InitializePieces();

	if ( myId == 0 ) {
		tc->ResetAllCameras ();

		qApp->exec();
		tc->StopServices();
	}
	else {
		wm->renderView->GetRenderWindow()->HideCursor();
		wm->renderView->GetRenderWindow()->BordersOff();
		wm->renderView->GetRenderWindow()->SetPosition(0,0);
		wm->renderView->GetRenderWindow()->SetSize(10,10);
		wm->renderView->GetRenderWindow()->OffScreenRenderingOn();
		wm->renderView->GetRenderWindow()->DoubleBufferOn();
		wm->renderView->GetRenderWindow()->EraseOn();

		tc->StartInteractor();
		//tc->StartServices();
	}
}

int main(int argc, char* argv[])
{
	vtkMPIController* controller = vtkMPIController::New();

	controller->Initialize(&argc, &argv);
	vtkMultiProcessController::SetGlobalController(controller);

	QVTKApplication app(argc, argv);

	//ka::Community com = ka::System::join_community(qApp->argc(), qApp->argv());
	/*
	ka::Community com = ka::System::join_community(argc, argv);
	com.commit();
	*/

	//win.selfRotate(5);
	//win.show();

	if (controller->IsA("vtkThreadedController"))
	{
		controller->SetNumberOfProcesses(4);
	}

	if ( controller->GetNumberOfProcesses() < 2 )
	{
		cerr << "This program requires more than one processor." << endl;
		return 1;
	}

	if ( controller->GetNumberOfProcesses() < 2 )
	{
		cerr << "This program requires more than one processor." << endl;
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

	/*
	com.leave();
	ka::System::terminate();
	*/

	controller->Finalize();
	vtkMultiProcessController::SetGlobalController(NULL);
	controller->Delete();

	return 0;
}

