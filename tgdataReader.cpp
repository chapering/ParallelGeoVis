// ----------------------------------------------------------------------------
// tgdataReader.cpp : an extension to vtkPolyData capable of reading tgdata 
//					geometry
//
// Creation : Nov. 13th 2011
//
// Copyright(C) 2011-2012 Haipeng Cai
//
// ----------------------------------------------------------------------------
#include "tgdataReader.h"
#include "GLoader.h"

#include <vtkPolyLine.h>

#include <vector>
#include <iostream>

using namespace std;

vtkTgDataReader::vtkTgDataReader()
{
	//allPoints = vtkSmartPointer<vtkPoints>::New();
	//allLines = vtkSmartPointer<vtkCellArray>::New();

	__polydata = vtkSmartPointer<vtkPolyData>::New();
}

vtkTgDataReader::~vtkTgDataReader()
{
}

void vtkTgDataReader::SetParallelParams(int numproc, int procid)
{
	m_numproc = numproc;
	m_procid = procid;
}

bool vtkTgDataReader::Load(const char* fndata)
{
	vtkSmartPointer<vtkPoints> allPoints = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkCellArray> allLines = vtkSmartPointer<vtkCellArray>::New();

	CTgdataLoader m_loader;
	if ( 0 != m_loader.load(fndata) ) {
		cout << "Loading geometry failed - GLApp aborted abnormally.\n";
		return false;
	}

	int startPtId = 0;

	unsigned long szTotal = m_loader.getSize();

	unsigned long sidx, eidx;

	sidx = szTotal / m_numproc * m_procid;

	if ( m_procid == m_numproc - 1 ) {
		eidx = szTotal;
	}
	else {
		eidx = szTotal / m_numproc * (m_procid + 1);
	}

	for (unsigned long idx = sidx; idx < eidx; ++idx) {
		const vector<GLfloat> & line = m_loader.getElement( idx );
		unsigned long szPts = static_cast<unsigned long>( line.size()/6 );
		GLfloat x,y,z;

		vtkSmartPointer<vtkPolyLine> vtkln = vtkSmartPointer<vtkPolyLine>::New();

		vtkln->GetPointIds()->SetNumberOfIds(szPts);
		for (unsigned long idx = 0; idx < szPts; idx++) {
			x = line [ idx*6 + 3 ], 
			y = line [ idx*6 + 4 ], 
			z = line [ idx*6 + 5 ];

			allPoints->InsertNextPoint( x, y, z );
			vtkln->GetPointIds()->SetId( idx, idx + startPtId );
		}
		allLines->InsertNextCell( vtkln );
		startPtId += szPts;
	}

	//this->SetPoints( allPoints );
	//this->SetLines( allLines );
	__polydata->SetPoints( allPoints );
	__polydata->SetLines( allLines );

	return true;
}

/* sts=8 ts=8 sw=80 tw=8 */

