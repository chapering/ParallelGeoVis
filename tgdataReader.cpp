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

vtkTgDataReader::vtkTgDataReader() :
	m_numproc (1),
	m_procid (0)
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

	vtkIdType szTotal = __polydata->GetNumberOfCells();

	vtkIdType sidx, eidx;

	sidx = szTotal / m_numproc * m_procid;

	if ( m_procid == m_numproc - 1 ) {
		eidx = szTotal;
	}
	else {
		eidx = szTotal / m_numproc * (m_procid + 1);
	}

	vtkCellArray* allLines = __polydata->GetLines();
	vtkSmartPointer<vtkCellArray> newallLines = vtkSmartPointer<vtkCellArray>::New();
	
	vtkIdType npts, *line;
	allLines->InitTraversal();
	vtkIdType idx = 0;
	while ( allLines->GetNextCell ( npts, line) ) {
		if (idx >= sidx && idx < eidx ) {
			newallLines->InsertNextCell(npts, line);
		}
		idx ++;
	}

	__polydata->SetLines ( newallLines );

	/*
	szTotal = __polydata->GetNumberOfCells();
	szTotal = __polydata->GetNumberOfPoints();
	cout << "szTotal = " << szTotal << "\n";
	*/
}

bool vtkTgDataReader::Load(const char* fndata)
{
	vtkSmartPointer<vtkPoints> allPoints = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkCellArray> allLines = vtkSmartPointer<vtkCellArray>::New();

	/*
	if ( m_procid == 0 ) {

		__polydata->SetWholeExtent(-1000,1000,-1000,1000,0,1000);
		__polydata->SetWholeBoundingBox(-1000,1000,-1000,1000,0,1000);
		__polydata->SetUpdateExtent (-1000,1000,-1000,1000,0,1000);
		return true;
	}

	m_numproc --;
	m_procid --;
	*/

	/*
	allPoints->SetNumberOfPoints(0);
	allLines->SetNumberOfCells(0);
	*/
	CTgdataLoader m_loader;
	if ( 0 != m_loader.load(fndata) ) {
		cout << "Loading geometry failed - GLApp aborted abnormally.\n";
		return false;
	}

	int startPtId = 0;

	unsigned long szTotal = m_loader.getSize();

	for (unsigned long idx = 0; idx < szTotal; ++idx) {
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

