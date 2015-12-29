#include "QVTKApplication.h"
#include "vmRenderWindow.h"

//#include "kaapi++"

/* extend RMI messages */
#define RENDER_RMI_CAMERA_UPDATE 87840
#define RENDER_RMI_MAPPING_UPDATE 87841

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
	  actor->GetMapper()->GetInput()->Modified();
	  //vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->GetOutput()->Modified();
	  //vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->GetInput()->Modified();
	  //actor->GetMapper()->ImmediateModeRenderingOn();
	  //vtkOpenGLPolyDataMapper::SafeDownCast(actor->GetMapper())->Draw( self->m_render, actor );

	  //actor->GetMapper()->Update();
	  //cout << "mapper updated.\n";
	  //actor->GetMapper()->GetInput()->InvokeEvent(vtkCommand::ModifiedEvent,NULL);

  }
}

int main(int argc, char* argv[])
{
	//ka::Community com = ka::System::join_community(qApp->argc(), qApp->argv());
	/*
	ka::Community com = ka::System::join_community(argc, argv);
	com.commit();
	*/

	vtkMPIController* controller = vtkMPIController::New();

	QVTKApplication app(argc, argv);

	controller->Initialize(&argc, &argv);
	vtkMultiProcessController::SetGlobalController(controller);

	if (controller->IsA("vtkThreadedController"))
	{
		controller->SetNumberOfProcesses(4);
	}

	if ( controller->GetNumberOfProcesses() < 2 )
	{
		cerr << "This program requires more than one processor." << endl;
		return 1;
	}

	CLegiMainWindow* win = new CLegiMainWindow (controller);
	win->setWindowTitle( "LegiDTI 1.0.0" );

	unsigned long mappingRMIid = controller->AddRMI( ::UpdateMapping, win, RENDER_RMI_MAPPING_UPDATE );

	//win.selfRotate(5);
	win->show();

	//---------------------------------------------------------------------------
	// Now start the event loop on the root node, on the satellites, we start the
	// vtkMultiProcessController::ProcessRMIs() so those processes start listening
	// to commands from the root-node.

	if (controller->GetLocalProcessId()==0)
	{
		app.exec();
		controller->TriggerBreakRMIs();
		controller->Barrier();
	}
	else
	{
		controller->ProcessRMIs();
		controller->Barrier();
	}

	/*
	com.leave();
	ka::System::terminate();
	*/

	controller->RemoveRMI( mappingRMIid );

	controller->Finalize();
	vtkMultiProcessController::SetGlobalController(NULL);
	controller->Delete();

	return 0;
}

