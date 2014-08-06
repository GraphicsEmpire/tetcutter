/*
 * WingedEdgeTetMesh.cpp
 *
 *  Created on: May 6, 2014
 *      Author: pourya
 */

#include <base/FileDirectory.h>
#include <base/Logger.h>
#include <base/MathBase.h>
#include <base/String.h>
#include <base/StringBase.h>
#include <base/Vec.h>
#include <deformable/VolMesh.h>
#include <graphics/AABB.h>

#include "graphics/selectgl.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

using namespace std;
using namespace PS;
using namespace PS::FILESTRINGUTILS;

using namespace PS;
using namespace PS::MESH;

VolMesh::VolMesh() {
	init();
}

VolMesh::VolMesh(U32 ctVertices, double* vertices, U32 ctElements, U32* elements) {
	init();
	setup(ctVertices, vertices, ctElements, elements);
}

VolMesh::VolMesh(const VolMesh& other) {
	init();

	vector<double> vertices;
	vector<U32> elements;

	U32 ctNodes = other.countNodes();
	U32 ctElements = other.countCells();
	vertices.resize(ctNodes * 3);
	elements.resize(ctElements * 4);

	//nodes
	for(U32 i=0; i<ctNodes; i++) {
		NODE node = other.const_nodeAt(i);
		vertices[i * 3 + 0] = node.pos.x;
		vertices[i * 3 + 1] = node.pos.y;
		vertices[i * 3 + 2] = node.pos.z;
	}

	//elements
	for(U32 i=0; i<ctElements; i++) {
		CELL elem = other.const_cellAt(i);
		elements[i * 4 + 0] = elem.nodes[0];
		elements[i * 4 + 1] = elem.nodes[1];
		elements[i * 4 + 2] = elem.nodes[2];
		elements[i * 4 + 3] = elem.nodes[3];
	}

	setup(vertices, elements);
}

VolMesh::VolMesh(const vector<double>& vertices, const vector<U32>& elements) {
	init();
	setup(vertices, elements);
}


VolMesh::~VolMesh() {
	cleanup();
}


VolMesh* VolMesh::CreateOneTet() {
	vector<double> vertices;
	vector<U32> elements;

	vec3d points[4];
	points[0] = vec3d(-1, 0, 0);
	points[1] = vec3d(0, 0, -2);
	points[2] = vec3d(1, 0, 0);
	points[3] = vec3d(0, 2, -1);

	vertices.resize(4 * 3);
	for (int i = 0; i < 4; i++) {
		points[i].store(&vertices[i * 3]);
	}

	elements.resize(4);
	elements[0] = 0;
	elements[1] = 1;
	elements[2] = 2;
	elements[3] = 3;

	VolMesh* tet = new VolMesh(vertices, elements);
	return tet;
}

void VolMesh::init() {
	m_elemToShow = INVALID_INDEX;
	m_fOnNodeEvent = NULL;
	m_fOnEdgeEvent = NULL;
	m_fOnFaceEvent = NULL;
	m_fOnElementEvent = NULL;
}

void VolMesh::setOnNodeEventCallback(OnNodeEvent f) {
	m_fOnNodeEvent = f;
}

void VolMesh::setOnEdgeEventCallback(OnEdgeEvent f) {
	m_fOnEdgeEvent = f;
}

void VolMesh::setOnFaceEventCallback(OnFaceEvent f) {
	m_fOnFaceEvent = f;
}

void VolMesh::setOnElemEventCallback(OnCellEvent f) {
	m_fOnElementEvent = f;
}


bool VolMesh::setup(const vector<double>& vertices, const vector<U32>& elements) {
	U32 ctVertices = vertices.size() / 3;
	U32 ctElements = elements.size() / 4;

	return setup(ctVertices, &vertices[0], ctElements, &elements[0]);
}

bool VolMesh::setup(U32 ctVertices, const double* vertices, U32 ctElements, const U32* elements) {

	//cleanup to setup the mesh
	cleanup();

	//add all vertices first
	for(U32 i=0; i<ctVertices; i++) {
		NODE node;
		node.pos = node.restpos = vec3d(&vertices[i*3]);
		insert_node(node);
	}


	//insert element
	for(U32 i=0; i<ctElements; i++)
		insert_cell(const_cast<U32 *>(&elements[i * 4]));


	//Compute AABB
	computeAABB();
	//checkMeshFaceDirections();
	this->printInfo();

	return true;
}

void VolMesh::cleanup() {
	m_vCells.resize(0);
	m_vFaces.resize(0);
	m_vEdges.resize(0);
	m_vNodes.resize(0);

	m_mapEdgesIndex.clear();
	m_incident_cells_per_face.clear();
	m_incident_edges_per_node.clear();
	m_incident_faces_per_edge.clear();
}

void VolMesh::printNodeInfo() const {
	//print all nodes
	printf("NODES #%u\n", countNodes());
	for(U32 i=0; i < countNodes(); i++) {
		const NODE& n = const_nodeAt(i);

		vector<U32> vIncidentEdges;
		getNodeIncidentEdges(i, vIncidentEdges);

		printf("NODE %u, incident edges count: %u, pos: [%.3f, %.3f, %.3f]\n", i, (U32)vIncidentEdges.size(), n.pos.x, n.pos.y, n.pos.z);
	}
	printf("\n");
}

void VolMesh::printEdgeInfo() const {
	//print all edges
	printf("EDGES #%u\n", countEdges());
	for(U32 i=0; i < countEdges(); i++) {
		const EDGE& e = const_edgeAt(i);

		printf("EDGE %u, from: %d, to %d\n", i, e.from, e.to);
	}
	printf("\n");
}

void VolMesh::printFaceInfo() const {
	//print all face
	printf("FACES #%u\n", countFaces());
	U32 vhandles[3];
	for(U32 i=0; i < countFaces(); i++) {
		const FACE& face = const_faceAt(i);
		getFaceNodes(i, vhandles);

		printf("FACE %u, Nodes [%u, %u, %u], Edges [%u, %u, %u]\n", i,
				vhandles[0], vhandles[1], vhandles[2],
				face.edges[0], face.edges[1], face.edges[2]);
	}
	printf("\n");
}

void VolMesh::printCellInfo() const {
	//print all elements
	printf("CELLS #%u\n", countCells());
	for(U32 i=0; i < countCells(); i++) {

		const CELL& cell = const_cellAt(i);
		printf("CELL %d, NODE: [%u, %u, %u, %u], ", i ,cell.nodes[0], cell.nodes[1], cell.nodes[2], cell.nodes[3]);
		printf("FACE: [%u, %u, %u, %u], ", cell.faces[0], cell.faces[1], cell.faces[2], cell.faces[3]);

		//print edges
		printf("EDGES: [");
		for(int e=0; e<6; e++) {
			if(e < 5)
				printf("%u, ", cell.edges[e]);
			else
				printf("%u]\n", cell.edges[e]);
		}
	}
	printf("\n");
}

void VolMesh::printInfo() const {
	printNodeInfo();
	printEdgeInfo();
	printFaceInfo();
	printCellInfo();
}

double VolMesh::computeDeterminant(U32 idxNodes[4]) const {
	vec3d v[4];

	for(int i=0; i<4; i++)
		v[i] = const_nodeAt(i).pos;
	return ComputeElementDeterminant(v);
}

double VolMesh::ComputeElementDeterminant(const vec3d v[4]) {
	return	vec3d::dot(v[1] - v[0], vec3d::cross(v[2] - v[0], v[3] - v[0]));
}

//add/remove
bool VolMesh::insert_cell(const CELL& cell) {

	//check the structure before adding it
	for(int i=0; i<4; i++) {
		if(!isFaceIndex(cell.faces[i])) {
			LogErrorArg2("Unable to add element. Invalid face index found at: f[%d] = %u", i, cell.faces[i]);
			return false;
		}
	}

	for(int i=0; i<4; i++) {
		if(!isNodeIndex(cell.nodes[i])) {
			LogErrorArg2("Unable to add element. Invalid node index found at: n[%d] = %u", i, cell.nodes[i]);
			return false;
		}
	}

	for(int i=0; i<6; i++) {
		if(!isEdgeIndex(cell.edges[i])) {
			LogErrorArg2("Unable to add element. Invalid edge index found at: edge[%d] = %u", i, cell.edges[i]);
			return false;
		}
	}

	m_vCells.push_back(cell);
	U32 idxCell = countCells() - 1;

	//update
	for(int i=0; i < 4; i++)
		m_incident_cells_per_face[ cell.faces[i] ].push_back(idxCell);

	if(m_fOnElementEvent)
		m_fOnElementEvent(cell, m_vCells.size() - 1, teAdded);

	return true;
}

bool VolMesh::insert_cell(U32 nodes[4]) {
	//Inserts a tetrahedra element to the halfedged mesh structure
	for(int i=0; i<4; i++) {
		if(!isNodeIndex(nodes[i])) {
			LogErrorArg1("Invalid node index passed in. %u", nodes[i]);
			return false;
		}
	}

	//face mask
	const int maskTetFaceNodes[4][3] = { {1, 2, 3}, {2, 0, 3}, {3, 0, 1}, {1, 0, 2} };
	const int maskTetFaceEdges[4][3] = { {0, 1, 2}, {3, 4, 1}, {4, 5, 2}, {5, 3, 0} };
	//const int faceMaskNeg[4][3] = { {3, 2, 1}, {3, 0, 2}, {1, 0, 3}, {2, 0, 1} };

	//edge mask
	const int maskTetEdges[6][2] = { {1, 2}, {2, 3}, {3, 1}, {2, 0}, {0, 3}, {0, 1} };
	//const int edgeMaskNeg[6][2] = { {3, 2}, {2, 1}, {1, 3}, {3, 0}, {0, 2}, {1, 0} };

	//maps for half edges
	std::map< EdgeKey, EDGE > mapEdges;
	typedef std::map< EdgeKey, EDGE >::iterator HEITER;

	//Add element nodes set only for now
	CELL cell;
	for(int i=0; i<4; i++)
		cell.nodes[i] = nodes[i];
	for(int i=0; i<4; i++)
		cell.faces[i] = INVALID_INDEX;
	for(int i=0; i<6; i++)
		cell.edges[i] = INVALID_INDEX;

	//Loop over faces. Per each tet 6 edges or 12 half-edges added
	for (int f = 0; f < 4; f++) {
		U32 fv[3];

		fv[0] = nodes[maskTetFaceNodes[f][0]];
		fv[1] = nodes[maskTetFaceNodes[f][1]];
		fv[2] = nodes[maskTetFaceNodes[f][2]];

		//Add all edges for this face
		for (int e = 0; e < 3; e++) {
			U32 from = fv[e];
			U32 to = fv[(e + 1) % 3];

			if(edge_exists(from, to) == false) {
				EdgeKey key(from, to);
				EDGE edge;
				edge.from = from;
				edge.to = to;
				mapEdges.insert(make_pair(key, edge));
			}
		}
	}

	//add final edges
	U32 from, to = INVALID_INDEX;

	while(mapEdges.size() > 0) {
		//add forward
		HEITER forward = mapEdges.begin();
		from = forward->second.from;
		to = forward->second.to;

		insert_edge(forward->second);

		//Remove from map
		mapEdges.erase(forward);
	}

	//add all faces now and finish setting information for all
	//loop over faces. Per each tet 6 edges or 12 half-edges added
	for (int f = 0; f < 4; f++) {
		U32 facenodes[3];

		facenodes[0] = nodes[maskTetFaceNodes[f][0]];
		facenodes[1] = nodes[maskTetFaceNodes[f][1]];
		facenodes[2] = nodes[maskTetFaceNodes[f][2]];

		//Set element face index
		cell.faces[f] = insert_face(facenodes);
	}

	//Set element edges
	for (int e = 0; e < 6; e++) {
		from = cell.nodes[maskTetEdges[e][0]];
		to = cell.nodes[maskTetEdges[e][1]];

		EdgeKey key(from, to);
		MAPHEDGEINDEXITER it = m_mapEdgesIndex.find(key);
		if (it != m_mapEdgesIndex.end())
			cell.edges[e] = it->second;
		else {
			LogErrorArg2("Setting element edges failed! Unable to find edge <%d, %d>",
						from, to);
			return false;
		}
	}

	//call simple insert now
	return insert_cell(cell);
}

void VolMesh::set_edge(U32 idxEdge, U32 from, U32 to) {
	assert(isEdgeIndex(idxEdge));

	//edge e
	EDGE& e = edgeAt(idxEdge);

	//remove incident edge idxEdge from the list of incident edges of the from node
	m_incident_edges_per_node[e.from].erase(std::remove(
			m_incident_edges_per_node[e.from].begin(),
			m_incident_edges_per_node[e.from].end(),
			idxEdge), m_incident_edges_per_node[e.from].end());

	//remove incident edge idxEdge from the list of incident edges of the to node
	m_incident_edges_per_node[e.to].erase(std::remove(
			m_incident_edges_per_node[e.to].begin(),
			m_incident_edges_per_node[e.to].end(),
			idxEdge), m_incident_edges_per_node[e.to].end());

	//remove edge from map
	removeEdgeIndexFromMap(e.from, e.to);

	//update
	e.from = from;
	e.to = to;
	m_vEdges[idxEdge] = e;

	//add to incident edges per node
	m_incident_edges_per_node[e.from].push_back(idxEdge);
	m_incident_edges_per_node[e.to].push_back(idxEdge);
	insertEdgeIndexToMap(e.from, e.to, idxEdge);
}

void VolMesh::set_face(U32 idxFace, U32 edges[3]) {

	assert(isFaceIndex(idxFace));

	FACE& face = faceAt(idxFace);

	//remove idxFace from the list of incident faces of faceedge0
	for(int i=0; i<COUNT_FACE_EDGES; i++) {
		m_incident_faces_per_edge[ face.edges[i] ].erase(
				std::remove(m_incident_faces_per_edge[ face.edges[i] ].begin(),
				m_incident_faces_per_edge[ face.edges[i] ].end(), idxFace),
				m_incident_faces_per_edge[ face.edges[i] ].end());
	}

	//UPDATE
	face.edges[0] = edges[0];
	face.edges[1] = edges[1];
	face.edges[2] = edges[2];
	m_vFaces[idxFace] = face;

	//add to incident faces per edge
	for(int i=0; i<COUNT_FACE_EDGES; i++)
		m_incident_faces_per_edge[edges[i]].push_back(idxFace);
}

void VolMesh::set_cell_faces(U32 idxCell, U32 faces[4]) {
	assert(isCellIndex(idxCell));

	CELL& cell = cellAt(idxCell);
	for(int i=0; i<4; i++) {
		m_incident_cells_per_face[ cell.faces[i] ].erase(
			std::remove(m_incident_cells_per_face[ cell.faces[i] ].begin(),
						m_incident_cells_per_face[ cell.faces[i] ].end(), idxCell),
			m_incident_cells_per_face[ cell.faces[i] ].end());
	}

	//UPDATE
	for(int i=0; i<4; i++)
		cell.faces[i] = faces[i];
	m_vCells[idxCell] = cell;

	//add to incident cells per face
	for(int i=0; i<4; i++)
		m_incident_cells_per_face[faces[i]].push_back(idxCell);
}

void VolMesh::remove_cell_core(U32 idxCell) {
	assert(isCellIndex(idxCell));

	//1. remove cell from the list of incident cell per face
	const CELL& cell = const_cellAt(idxCell);
	for(int i=0; i<4; i++) {
		m_incident_cells_per_face[ cell.faces[i] ].erase(
				std::remove(m_incident_cells_per_face[ cell.faces[i] ].begin(),
				m_incident_cells_per_face[ cell.faces[i] ].end(), idxCell),
				m_incident_cells_per_face[ cell.faces[i] ].end());
	}

	//2. correct all referenced cell indices
    HandleCorrection corrector(idxCell);
    std::for_each(m_incident_cells_per_face.begin(),
                  m_incident_cells_per_face.end(),
                  std::bind(&HandleCorrection::correctVecValue, &corrector, std::placeholders::_1));


	//remove the cell from list
	m_vCells.erase(m_vCells.begin() + idxCell);
}

void VolMesh::remove_face_core(U32 idxFace) {
	assert(isFaceIndex(idxFace));

	//1. remove from incident faces per edge
	//2. Decrease all face handles > idxFace in incident cells
	//3. Delete entry in bottom-up list: cells per face
	//4. Decrease all face handles > idxFace in edge bottom up list: incident faces per edge
	//5. Delete face itself

	//1. remove from incident faces
	const FACE& face = const_faceAt(idxFace);
	for(U32 i=0; i < COUNT_FACE_EDGES; i++) {
		U32 idxEdge = face.edges[i];

		m_incident_faces_per_edge[idxEdge].erase(
				std::remove( m_incident_faces_per_edge[idxEdge].begin(),
							 m_incident_faces_per_edge[idxEdge].end(), idxFace),
				m_incident_faces_per_edge[idxEdge].end());
	}

	//2. decrease all face handles > idxFace in incident cells
    // Decrease all half-face handles > _h in all cells
    // and delete all half-face handles == _h
    std::set<U32> update_cells;
    for(U32 i = idxFace; i < countFaces(); i++ ) {

    	for(std::vector<U32>::const_iterator c_it = m_incident_cells_per_face[i].begin();
    		c_it != m_incident_cells_per_face[i].end(); c_it++)

    		if(isCellIndex(*c_it))
    			update_cells.insert(*c_it);
    }

    //loop over update_cells
    for(std::set<U32>::const_iterator c_it = update_cells.begin(),
            c_end = update_cells.end(); c_it != c_end; ++c_it) {

    	CELL& cell = cellAt(*c_it);

    	//decrement face handles greater than idxFace
    	for(U32 i = 0; i < COUNT_CELL_FACES; i++) {
    		if(cell.faces[i] == idxFace)
    			cell.faces[i] = INVALID_INDEX;
    		else if(cell.faces[i] > idxFace)
    			cell.faces[i] --;
    	}

    	//cell faces
    	set_cell_faces(*c_it, cell.faces);
    }

    //3.delete entry
    m_incident_cells_per_face.erase(m_incident_cells_per_face.begin() + idxFace);

    //4.decrease all face handles per edge
    HandleCorrection cor(idxFace);
    std::for_each(m_incident_faces_per_edge.begin(),
    			  m_incident_faces_per_edge.end(),
                  std::bind(&HandleCorrection::correctVecValue, &cor, std::placeholders::_1));

    //4.delete face
    m_vFaces.erase(m_vFaces.begin() + idxFace);

}

void VolMesh::remove_edge_core(U32 idxEdge) {
	assert(isEdgeIndex(idxEdge));

	//1. Delete bottom-up links from incident edges per node
	//2. Decrease all edge handles > idxEdge in incident faces per edge
	//3. Delete entry in bottom-up list: incident faces per edge
	//4. Decrease all edge handles > idxEdge in incident edges per node
	//5. Delete edge itself
	const EDGE& edge = const_edgeAt(idxEdge);

	//1. bottomup links
	//remove idxEdge from the list of start node
	m_incident_edges_per_node[edge.from].erase(std::remove(m_incident_edges_per_node[edge.from].begin(),
											   m_incident_edges_per_node[edge.from].end(), idxEdge),
											   m_incident_edges_per_node[edge.from].end());

	//remove idxEdge from the list of end node
	m_incident_edges_per_node[edge.to].erase(std::remove(m_incident_edges_per_node[edge.to].begin(),
											   m_incident_edges_per_node[edge.to].end(), idxEdge),
											   m_incident_edges_per_node[edge.to].end());

	//2. decrease all edge handles > idxEdge in incident faces per edge
	std::set<U32> update_faces;
	for(U32 i=idxEdge; i < countEdges(); i++) {
		for(std::vector<U32>::const_iterator f_it = m_incident_faces_per_edge[i].begin();
			f_it != m_incident_faces_per_edge[i].end(); f_it++) {

			if(isFaceIndex(*f_it))
				update_faces.insert(*f_it);
		}
	}

	//update faces
	for(std::set<U32>::iterator f_it = update_faces.begin(),
		f_end = update_faces.end(); f_it != f_end; ++f_it) {

    	FACE& face = faceAt(*f_it);

    	//decrement edge handles greater than idxEdge
    	for(U32 i = 0; i < COUNT_FACE_EDGES; i++) {
    		if(face.edges[i] == idxEdge)
    			face.edges[i] = INVALID_INDEX;
    		else if(face.edges[i] > idxEdge)
    			face.edges[i] --;
    	}

    	set_face(*f_it, face.edges);
	}

	//update cells
	set<U32> update_cells;
	get_incident_cells(update_faces, update_cells);

	for (std::set<U32>::iterator c_it = update_cells.begin(), c_end =
			update_cells.end(); c_it != c_end; ++c_it) {

		CELL& cell = cellAt(*c_it);

		//decrement edge handles greater than idxEdge
		for (U32 i = 0; i < COUNT_CELL_EDGES; i++) {
			if (cell.edges[i] == idxEdge)
				cell.edges[i] = INVALID_INDEX;
			else if(cell.edges[i] > idxEdge)
				cell.edges[i] --;
		}

		m_vCells[*c_it] = cell;
	}




	//3. delete entry
	m_incident_faces_per_edge.erase(m_incident_faces_per_edge.begin() + idxEdge);

    //4.decrease all edges handles per node
    HandleCorrection cor(idxEdge);
    std::for_each(m_incident_edges_per_node.begin(),
    			  m_incident_edges_per_node.end(),
                  std::bind(&HandleCorrection::correctVecValue, &cor, std::placeholders::_1));

    //4.delete edge itself
    m_vEdges.erase(m_vEdges.begin() + idxEdge);
}

void VolMesh::remove_node_core(U32 idxNode) {
	assert(isNodeIndex(idxNode));

	//1. Decrease all vertex handles > idxNode in incident edges per node
	//2. Delete entry in bottom-up list: incident edges per node
	//3. Delete vertex itself (not necessary here since a vertex is only represented by a number)
	//4. Delete property entry

	//1.
	set<U32> setUpdateEdges;
	for(U32 i = idxNode; i < countNodes(); i++) {

		const vector<U32>& vEdges = m_incident_edges_per_node[i];

		for(std::vector<U32>::const_iterator e_it = vEdges.begin(),
			e_end = vEdges.end(); e_it != e_end; e_it++) {

			//insert the edge into
			setUpdateEdges.insert(*e_it);
			EDGE& e = edgeAt(*e_it);

			if(e.from == i)
				set_edge(*e_it, i - 1, e.to);
			else if(e.to == i)
				set_edge(*e_it, e.from, i - 1);
		}
	}


	//update cells
	set<U32> setUpdateCells;
	{
		set<U32> setUpdateFaces;
		get_incident_faces(setUpdateEdges, setUpdateFaces);
		get_incident_cells(setUpdateFaces, setUpdateCells);
	}

	//setUpdateCells
	for(set<U32>::iterator c_it = setUpdateCells.begin(); c_it != setUpdateCells.end(); c_it ++) {
		CELL& cell = cellAt(*c_it);

		for(U32 i = 0; i < COUNT_CELL_NODES; i++) {
			if(cell.nodes[i] == idxNode)
				cell.nodes[i] = INVALID_INDEX;
			else if(cell.nodes[i] > idxNode)
				cell.nodes[i] --;
		}

		m_vCells[*c_it] = cell;
	}


	//2. delete from incident edges per node
	m_incident_edges_per_node.erase(m_incident_edges_per_node.begin() + idxNode);

	//3. delete vertex
	m_vNodes.erase(m_vNodes.begin() + idxNode);

}

void VolMesh::remove_cell(U32 idxCell) {
	remove_cell_core(idxCell);
}

void VolMesh::remove_face(U32 idxFace) {

	vector<U32> vfaces;
	vfaces.push_back(idxFace);

	//get incident cells to the input face
	set<U32> outsetCells;
	get_incident_cells(vfaces, outsetCells);

	//remove all incident cells
	for(set<U32>::const_iterator it = outsetCells.begin(); it != outsetCells.end(); it++)
		remove_cell_core(*it);

	//remove the face itself
	remove_face_core(idxFace);
}

void VolMesh::remove_edge(U32 idxEdge) {
	vector<U32> vedges;
	vedges.push_back(idxEdge);

	//get incident faces to the input edge
	set<U32> outsetFaces;
	get_incident_faces(vedges, outsetFaces);

	//get incident cells to the outset faces
	set<U32> outsetCells;
	get_incident_cells(outsetFaces, outsetCells);

	//delete cells
	for(std::set<U32>::const_reverse_iterator c_it = outsetCells.rbegin(),
	    c_end = outsetCells.rend(); c_it != c_end; ++c_it ) {

		//remove cell
		remove_cell_core(*c_it);
	}

	//delete faces
	for(std::set<U32>::const_reverse_iterator f_it = outsetFaces.rbegin(),
	    c_end = outsetFaces.rend(); f_it != c_end; ++f_it ) {

		//remove face
		remove_face_core(*f_it);
	}

	remove_edge_core(idxEdge);
}

void VolMesh::remove_node(U32 idxNode) {
	vector<U32> vnodes;
	vnodes.push_back(idxNode);

	//get incident edges
	set<U32> outsetEdges;
	get_incident_edges(vnodes, outsetEdges);

	//get incident faces
	set<U32> outsetFaces;
	get_incident_faces(outsetEdges, outsetFaces);

	//get incident cells
	set<U32> outsetCells;
	get_incident_cells(outsetFaces, outsetCells);

	//delete cells
	for(std::set<U32>::const_reverse_iterator c_it = outsetCells.rbegin(),
	    c_end = outsetCells.rend(); c_it != c_end; ++c_it ) {

		//remove cell
		remove_cell_core(*c_it);
	}

	//delete faces
	for(std::set<U32>::const_reverse_iterator f_it = outsetFaces.rbegin(),
	    c_end = outsetFaces.rend(); f_it != c_end; ++f_it ) {

		//remove face
		remove_face_core(*f_it);
	}

	//delete edges
	for(std::set<U32>::const_reverse_iterator e_it = outsetEdges.rbegin(),
	    e_end = outsetEdges.rend(); e_it != e_end; ++e_it ) {

		//remove edge
		remove_edge_core(*e_it);
	}


	remove_node_core(idxNode);
}



U32 VolMesh::insert_node(const NODE& n) {
	m_vNodes.push_back(n);
	m_incident_edges_per_node.resize(countNodes());

	return countNodes() - 1;
}

U32 VolMesh::insert_edge(const EDGE& e) {

	assert(e.from != e.to);

	m_vEdges.push_back(e);
	m_incident_faces_per_edge.resize(countEdges());
	U32 idxEdge = countEdges() - 1;

	//update incident edges per vertex
	m_incident_edges_per_node[e.from].push_back(idxEdge);
	m_incident_edges_per_node[e.to].push_back(idxEdge);


	//insert the forward halfedge into map
	insertEdgeIndexToMap(e.from, e.to, idxEdge);

	return idxEdge;
}

//inserting a face uniquely
U32 VolMesh::insert_face(U32 nodes[3]) {

	U32 idxFace = face_handle_by_nodes(nodes);
	if(isFaceIndex(idxFace)) {
		return idxFace;
	}

	//since we do not have that face we will add it
	FACE face;
	U32 from, to = BaseHandle::INVALID;

	//Add all edges
	for (int e = 0; e < 3; e++) {
		from = nodes[e];
		to = nodes[(e + 1) % 3];

		EdgeKey key(from, to);
		MAPHEDGEINDEXITER it = m_mapEdgesIndex.find(key);
		if (it != m_mapEdgesIndex.end()) {
			face.edges[e] = it->second;
		}
		else {
			LogErrorArg2("Setting face edges failed! Unable to find edge <%d, %d>",
						 from, to);

			return BaseHandle::INVALID;
		}
	}

	//add the face now
	m_vFaces.push_back(face);
	idxFace = countFaces() - 1;
	m_incident_cells_per_face.resize(countFaces());

	//update
	for(int i=0; i < COUNT_FACE_EDGES; i++)
		m_incident_faces_per_edge[face.edges[i]].push_back(idxFace);

	return idxFace;
}


void VolMesh::garbage_collection() {

	//acquire lock to mesh

	std::set<U32> setToBeRemoved;

	//******************************************************
	//1. identify unused faces and remove them
	for(U32 i = 0; i < countFaces(); i++) {
		if(m_incident_cells_per_face[i].size() == 0) {
			setToBeRemoved.insert(i);
			printf("GC: face %u should be removed.\n", i);
		}
	}

	//remove batch of faces in increasing index order
	remove_faces(setToBeRemoved);
	U32 ctRemovedFaces = setToBeRemoved.size();

	//******************************************************
	//2. identify unused edges and remove them
	setToBeRemoved.clear();

/*
	for(U32 i = 0; i < countEdges(); i++) {
		if(m_incident_faces_per_edge[i].size() == 0) {
			setToBeRemoved.insert(i);
			printf("GC: edge %u should be removed.\n", i);
		}
	}

	//remove batch of edges in increasing index order
	remove_edges(setToBeRemoved);
	*/
	U32 ctRemovedEdges = setToBeRemoved.size();


	//******************************************************
	//3. identify unused nodes and remove them
	setToBeRemoved.clear();

	/*
	for(U32 i = 0; i < countNodes(); i++) {
		if(m_incident_edges_per_node[i].size() == 0) {
			setToBeRemoved.insert(i);
			printf("GC: node %u should be removed.\n", i);
		}
	}

	//remove batch of nodes in increasing index order
	remove_nodes(setToBeRemoved);

	*/
	U32 ctRemovedNodes = setToBeRemoved.size();
	setToBeRemoved.clear();

	LogInfoArg3("garbage collection removed: Faces# %u, Edges# %u, Nodes# %u",
				ctRemovedFaces, ctRemovedEdges, ctRemovedNodes);

	//release lock
}

bool VolMesh::getFaceNodes(U32 idxFace, U32 (&nodes)[3]) const {
	if(!isFaceIndex(idxFace))
		return false;

	const FACE& face = const_faceAt(idxFace);

	std::set<U32> setFaceNodes;
	for(int i=0; i < 3; i++) {
		setFaceNodes.insert(from_node(face.edges[i]));
		setFaceNodes.insert(to_node(face.edges[i]));
	}

	int i = 0;
	for(std::set<U32>::const_iterator it = setFaceNodes.begin(); it != setFaceNodes.end(); ++it) {
		nodes[i] = *it;
		i++;

		if(i == COUNT_FACE_EDGES)
			break;
	}

	return (setFaceNodes.size() == COUNT_FACE_EDGES);
}

CELL& VolMesh::cellAt(U32 i) {
	assert(isCellIndex(i));
	return m_vCells[i];
}

FACE& VolMesh::faceAt(U32 i) {
	assert(isFaceIndex(i));
	return m_vFaces[i];
}

EDGE& VolMesh::edgeAt(U32 i) {
	assert(isEdgeIndex(i));
	return m_vEdges[i];
}

NODE& VolMesh::nodeAt(U32 i) {
	assert(isNodeIndex(i));
	return m_vNodes[i];
}


//access
const CELL& VolMesh::const_cellAt(U32 i) const {
	assert(isCellIndex(i));
	return m_vCells[i];
}

const FACE& VolMesh::const_faceAt(U32 i) const {
	assert(isFaceIndex(i));
	return m_vFaces[i];
}

const EDGE& VolMesh::const_edgeAt(U32 i) const {
	assert(isEdgeIndex(i));
	return m_vEdges[i];
}

const NODE& VolMesh::const_nodeAt(U32 i) const {
	assert(isNodeIndex(i));
	return m_vNodes[i];
}

void VolMesh::displace(double * u) {
	if(u == NULL)
		return;

	for(U32 i=0; i < countNodes(); i++) {
		m_vNodes[i].pos = m_vNodes[i].pos + vec3d(&u[i * 3]);
	}

	computeAABB();
}

bool VolMesh::insertEdgeIndexToMap(U32 from, U32 to, U32 idxEdge) {
	if(from == to) {
		LogErrorArg2("Can not register an edge with the same from and to nodes! [%u, %u]", from, to);
		return false;
	}

	if(!isEdgeIndex(idxEdge)) {
		LogErrorArg1("Can not register an invalid edge index: %u", idxEdge);
		return false;
	}

	EdgeKey key(from, to);
	m_mapEdgesIndex.insert( std::make_pair( key, idxEdge) );
	return true;
}

bool VolMesh::removeEdgeIndexFromMap(U32 from, U32 to) {
	EdgeKey key(from, to);
	MAPHEDGEINDEXITER it = m_mapEdgesIndex.find(key);
	if(it == m_mapEdgesIndex.end())
		return false;
	else {
		m_mapEdgesIndex.erase(it);
		return true;
	}
}

EdgeKey VolMesh::computeEdgeKey(U32 idxEdge) const {
	assert(isEdgeIndex(idxEdge));

	const EDGE& e = const_edgeAt(idxEdge);
	EdgeKey key(e.from, e.to);
	return key;
}


bool VolMesh::edge_exists(U32 from, U32 to) {
	return isEdgeIndex(edge_handle(from, to));
}

U32 VolMesh::edge_handle(U32 from, U32 to) {
	EdgeKey key(from, to);
	MAPHEDGEINDEXITER it = m_mapEdgesIndex.find(key);
	if(it != m_mapEdgesIndex.end())
		return it->second;
	else
		return INVALID_INDEX;
}

bool VolMesh::face_exists_by_edges(U32 edges[3]) const {
	return isFaceIndex(face_handle_by_edges(edges));
}

bool VolMesh::face_exists_by_nodes(U32 nodes[3]) {
	return isFaceIndex(face_handle_by_nodes(nodes));
}

U32 VolMesh::face_handle_by_edges(U32 edges[3]) const {

	//edges 0 - 2 are used
	assert(isEdgeIndex(edges[0]) && isEdgeIndex(edges[1]) && isEdgeIndex(edges[2]));
	FaceKey query(&edges[0]);

	vector<U32> facesIncidentToEdge0;
	facesIncidentToEdge0.assign(m_incident_faces_per_edge[ edges[0] ].begin(), m_incident_faces_per_edge[ edges[0] ].end());

	for(U32 i = 0; i < facesIncidentToEdge0.size(); i++) {

		const FACE& face = const_faceAt(facesIncidentToEdge0[i]);
		FaceKey faceKey(const_cast<U32 *>(&face.edges[0]));

		if (query.key == faceKey.key)
			return facesIncidentToEdge0[i];
	}

	return INVALID_INDEX;
}

U32 VolMesh::face_handle_by_nodes(U32 nodes[3])  {

	assert(isNodeIndex(nodes[0]) && isNodeIndex(nodes[1]) && isNodeIndex(nodes[2]));

	U32 edges[3];
	edges[0] = edge_handle(nodes[0], nodes[1]);
	edges[1] = edge_handle(nodes[1], nodes[2]);
	edges[2] = edge_handle(nodes[2], nodes[0]);

	return face_handle_by_edges(edges);
}

template <class ContainerT>
int VolMesh::get_incident_cells(const ContainerT& in_faces, set<U32>& out_cells) {

	vector<U32> vTempCells;
	for(typename ContainerT::const_iterator f_it = in_faces.begin(),
            f_end = in_faces.end(); f_it != f_end; ++f_it) {

		vTempCells.assign(m_incident_cells_per_face[*f_it].begin(), m_incident_cells_per_face[*f_it].end());

		for(U32 j=0; j < vTempCells.size(); j++) {
			U32 idxCell = vTempCells[j];
			if(isCellIndex(idxCell))
				out_cells.insert(idxCell);
		}
	}

	return (int)out_cells.size();
}

template <class ContainerT>
int VolMesh::get_incident_faces(const ContainerT& in_edges, set<U32>& out_faces) {
	vector<U32> vTempFaces;

	for(typename ContainerT::const_iterator e_it = in_edges.begin(),
            e_end = in_edges.end(); e_it != e_end; ++e_it) {

		vTempFaces.assign(m_incident_faces_per_edge[*e_it].begin(), m_incident_faces_per_edge[*e_it].end());
		for(U32 j=0; j < vTempFaces.size(); j++) {
			U32 idxFace = vTempFaces[j];
			if(isFaceIndex(idxFace))
				out_faces.insert(idxFace);
		}
	}

	return (int)out_faces.size();
}

template <class ContainerT>
int VolMesh::get_incident_edges(const ContainerT& in_nodes, set<U32>& out_edges) {
	vector<U32> vTempEdges;
	for(typename ContainerT::const_iterator n_it = in_nodes.begin(),
	            n_end = in_nodes.end(); n_it != n_end; ++n_it) {

		vTempEdges.assign(m_incident_edges_per_node[*n_it].begin(), m_incident_faces_per_edge[*n_it].end());
		for(U32 j=0; j < vTempEdges.size(); j++) {
			U32 idxEdge = vTempEdges[j];
			if(isNodeIndex(idxEdge))
				out_edges.insert(idxEdge);
		}
	}

	return (int)out_edges.size();
}

template <class ContainerT>
void VolMesh::remove_cells(const ContainerT& cells) {
	std::vector<U32> vToBeRemoved;
	vToBeRemoved.resize(cells.size());
	std::copy(cells.begin(), cells.end(), vToBeRemoved.begin());
	std::sort(vToBeRemoved.begin(), vToBeRemoved.end(), std::greater<U32>());

	//print order
	printf("cells remove order: ");
	for(std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++)
		printf("%u, ", *it);
	printf("\n");

	for(std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++) {
		remove_cell(*it);
	}
}

template <class ContainerT>
void VolMesh::remove_faces(const ContainerT& faces) {
	std::vector<U32> vToBeRemoved;
	vToBeRemoved.resize(faces.size());
	std::copy(faces.begin(), faces.end(), vToBeRemoved.begin());
	std::sort(vToBeRemoved.begin(), vToBeRemoved.end(), std::greater<U32>());

	//print order
	printf("faces remove order: ");
	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++)
		printf("%u, ", *it);
	printf("\n");

	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++) {
		remove_face(*it);
	}

}

template <class ContainerT>
void VolMesh::remove_edges(const ContainerT& edges) {
	std::vector<U32> vToBeRemoved;
	vToBeRemoved.resize(edges.size());
	std::copy(edges.begin(), edges.end(), vToBeRemoved.begin());
	std::sort(vToBeRemoved.begin(), vToBeRemoved.end(), std::greater<U32>());

	//print order
	printf("edges remove order: ");
	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++)
		printf("%u, ", *it);
	printf("\n");

	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++) {
		remove_edge(*it);
	}
}

template <class ContainerT>
void VolMesh::remove_nodes(const ContainerT& nodes) {
	std::vector<U32> vToBeRemoved;
	vToBeRemoved.resize(nodes.size());
	std::copy(nodes.begin(), nodes.end(), vToBeRemoved.begin());
	std::sort(vToBeRemoved.begin(), vToBeRemoved.end(), std::greater<U32>());

	//print order
	printf("nodes remove order: ");
	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++)
		printf("%u, ", *it);
	printf("\n");

	for (std::vector<U32>::const_iterator it = vToBeRemoved.begin(); it != vToBeRemoved.end(); it++) {
		remove_node(*it);
	}
}

bool VolMesh::cut_edge(int idxEdge, double distance, U32* poutIndexNP0, U32* poutIndexNP1) {
	if(!isEdgeIndex(idxEdge))
		return false;

	U32 from = from_node(idxEdge);
	U32 to = to_node(idxEdge);

	const NODE& p0 = const_nodeAt(from);
	const NODE& p1 = const_nodeAt(to);

	//add two new points np0 and np1
	NODE np0;
	np0.pos = p0.pos + (p1.pos - p0.pos).normalized() * distance;
	np0.restpos = p0.restpos + (p1.restpos - p0.restpos).normalized() * distance;
	U32 idxNP0 = insert_node(np0);

	NODE np1 = np0;
	U32 idxNP1 = insert_node(np1);

	//update the old edge
	set_edge(idxEdge, from, idxNP0);


	//add the new edge
	EDGE ne0(idxNP1, to);
	insert_edge(ne0);



	//Write output vertices
	if(poutIndexNP0)
		*poutIndexNP0 = idxNP0;

	if(poutIndexNP1)
		*poutIndexNP1 = idxNP1;

	return true;
}

bool VolMesh::getNodeIncidentEdges(U32 idxNode, vector<U32>& incidentEdges) const {

	if(!isNodeIndex(idxNode))
		return false;

	incidentEdges.assign(m_incident_edges_per_node[idxNode].begin(), m_incident_edges_per_node[idxNode].end());
	return true;
}

bool VolMesh::getCellFacesExpensive(U32 idxCell, U32 (&faces)[4]) {
	if(!isCellIndex(idxCell))
		return false;

	//face mask
	const int faceMaskPos[4][3] = { {1, 2, 3}, {2, 0, 3}, {3, 0, 1}, {1, 0, 2} };

	const CELL& cell = const_cellAt(idxCell);
	for(U32 i=0; i<4; i++) {

		U32 nh[3];
		nh[0] = cell.nodes[faceMaskPos[i][0]];
		nh[1] = cell.nodes[faceMaskPos[i][1]];
		nh[2] = cell.nodes[faceMaskPos[i][2]];

		faces[i] = face_handle_by_nodes(nh);
	}

	return true;
}

bool VolMesh::getCellEdgesExpensive(U32 idxCell, U32 (&edges)[6])  {
	//edge mask
	static int edgeMaskPos[6][2] = { {1, 2}, {2, 3}, {3, 1}, {2, 0}, {0, 3}, {0, 1} };

	const CELL& cell = const_cellAt(idxCell);
	for(U32 i=0; i<6; i++) {
		edges[i] = edge_handle(cell.nodes[edgeMaskPos[i][0]], cell.nodes[edgeMaskPos[i][1]]);
	}

	return true;
}



void VolMesh::setElemToShow(U32 elem) {
	if(elem == m_elemToShow)
		return;
	LogInfoArg1("Requested element to shows: %u\n", elem);
	m_elemToShow = elem;
}


bool VolMesh::readVegaFormat(const AnsiStr& strFP) {
	if(!FileExists(strFP))
		return false;
	char chrLine[1024];
	ifstream fpIn(strFP.cptr());
	if(!fpIn.is_open())
		return false;

	vector<double> vertices;
	vector<U32> elements;

	U32 ctLine = 0;
	U32 idxVertex = 0;
	U32 idxElements = 0;

	enum READMODE {modeReadHeader, modeReadVertex, modeReadElements};

	READMODE mode = modeReadHeader;
	READMODE prevMode = mode;

	while(!fpIn.eof())
	{
		fpIn.getline(chrLine, 1024);

		AnsiStr strLine(chrLine);
		strLine.removeStartEndSpaces();
		strLine.trim();
		if(strLine.firstChar() == '#')
			continue;
		if(strLine.firstChar() == '*') {
			if(strLine == "*VERTICES")
				mode = modeReadVertex;
			else if(strLine == "*ELEMENTS")
				mode = modeReadElements;
			continue;
		}

		if(strLine.length() == 0)
			break;

		if(prevMode != mode) {
			if(mode == modeReadVertex && idxVertex == 0)
			{
				int ctVertices;
				int ctCoords;
				sscanf(strLine.cptr(), "%d %d", &ctVertices, &ctCoords);
				if(ctVertices <= 0 || ctCoords != 3) {
					fpIn.close();
					return false;
				}

				vertices.reserve(ctVertices * 3);
			}
			else if(mode == modeReadElements && idxElements == 0) {
				int ctElems;
				int ctPerElemNodes;
				sscanf(strLine.cptr(), "%d %d", &ctElems, &ctPerElemNodes);
				if(ctElems <= 0 || ctPerElemNodes != 4) {
					fpIn.close();
					return false;
				}

				elements.reserve(ctElems * 4);
			}
		}


		if(mode == modeReadVertex)
		{
			vec3d v;
			vector<AnsiStr> arrWords;
			int ctWords = strLine.decompose(' ', arrWords);
			if(ctWords >= 4)
			{
				//int temp = atoi(arrWords[0].cptr());
				v.x = atof(arrWords[1].cptr());
				v.y = atof(arrWords[2].cptr());
				v.z = atof(arrWords[3].cptr());
			}

			//add vertex to the array
			vertices.push_back(v.x);
			vertices.push_back(v.y);
			vertices.push_back(v.z);
			idxVertex++;
		}
		else if(mode == modeReadElements) {
			vec4u32 e;
			vector<AnsiStr> arrWords;
			int ctWords = strLine.decompose(' ', arrWords);
			if(ctWords >= 4)
			{
				//int temp = atoi(arrWords[0].cptr());
				e.x = atoi(arrWords[1].cptr());
				e.y = atoi(arrWords[2].cptr());
				e.z = atoi(arrWords[3].cptr());
				e.w = atoi(arrWords[4].cptr());
			}

			//add element to the array
			elements.push_back(e.x);
			elements.push_back(e.y);
			elements.push_back(e.z);
			elements.push_back(e.w);

			idxElements++;
		}

		prevMode = mode;
		ctLine++;
	}
	fpIn.close();


	//setup
	cleanup();
	return setup(vertices, elements);
}

bool VolMesh::writeVegaFormat(const AnsiStr& strFP) const {
	if(countNodes() == 0 || countCells() == 0)
		return false;

	//Output veg file
	AnsiStr strVegFP = ChangeFileExt(strFP, ".veg");
	ofstream fpOut(strVegFP.cptr());

	//Include Node File
	fpOut << "# Vega Mesh File, Generated by FemBrain.\n";
	fpOut << "# " << countNodes() << " vertices, " << countCells() << " elements\n";
	fpOut << "\n";
	fpOut << "*VERTICES\n";
	fpOut << countNodes() << " 3 0 0\n";

	for(U32 i=0; i < countNodes(); i++)
	{
		vec3d v = const_nodeAt(i).pos;

		//VEGA expects one based index for everything
		fpOut << i+1 << " " << v.x << " " << v.y << " " << v.z << "\n";
	}

	//Line Separator
	fpOut << "\n";
	fpOut << "*ELEMENTS\n";
	fpOut << "TET\n";
	fpOut << countCells() << " 4 0\n";

	for(U32 i=0; i < countCells(); i++)
	{
		CELL elem = const_cellAt(i);
		vec4u32 n = vec4u32(&elem.nodes[0]);

		//VEGA expects one based index for everything
		fpOut << i+1 << " " << n.x+1 << " " << n.y+1 << " " << n.z+1 << " " << n.w+1 << "\n";
	}

	//Add Default Material
	fpOut << "\n";
	fpOut << "*MATERIAL BODY\n";
	fpOut << "ENU, 1000, 10000000, 0.45\n";

	fpOut << "\n";
	fpOut << "*REGION\n";
	fpOut << "allElements, BODY\n";

	//Include Element File
	fpOut.close();

	return true;
}


void VolMesh::draw() {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_LIGHTING);

	const U8 ctColors = 7;
	vec3d colors[ctColors] = {vec3d(0.5, 0.5, 0.5), vec3d(1, 0.3, 0), vec3d(0.3, 1, 0), vec3d(0, 0.3, 1),
					   	   	   vec3d(1, 1, 0), vec3d(0.5, 0.5, 0.5), vec3d(0, 1, 1)};

	//Draw filled faces
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_CULL_FACE);
	if(isCellIndex(m_elemToShow))
	{
		glColor3dv(colors[m_elemToShow % ctColors].cptr());
		drawElement(m_elemToShow);
	}
	else {
		for(U32 i=0; i< countCells(); i++)
		{
			glColor3dv(colors[i % ctColors].cptr());
			drawElement(i);
		}
	}
	glEnable(GL_CULL_FACE);


	//Draw outlined faces
	glDisable(GL_CULL_FACE);
	glLineWidth(3.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glColor3f(0.0f, 0.0f, 0.0f);

	for(U32 i=0; i< countCells(); i++) {
		drawElement(i);
	}
	glEnable(GL_CULL_FACE);

	//Draw vertices
	glPointSize(5.0f);
	glColor3f(1.0f, 0.0f, 0.0f);
	glBegin(GL_POINTS);
	for(U32 i=0; i < m_vNodes.size(); i++) {
		NODE n = const_nodeAt(i);
		glVertex3dv(n.pos.cptr());
	}
	glEnd();

	glEnable(GL_LIGHTING);
	glPopAttrib();
}

void VolMesh::drawElement(U32 i) const {
	if(!isCellIndex(i))
		return;

	const CELL& cell = const_cellAt(i);

	glBegin(GL_TRIANGLES);
		for(U32 f=0; f<4; f++)
		{
			U32 nodes[3];
			getFaceNodes(cell.faces[f], nodes);

			vec3d p0 = const_nodeAt(nodes[0]).pos;
			vec3d p1 = const_nodeAt(nodes[1]).pos;
			vec3d p2 = const_nodeAt(nodes[2]).pos;

			glVertex3dv(p0.cptr());
			glVertex3dv(p1.cptr());
			glVertex3dv(p2.cptr());
		}
	glEnd();
}


AABB VolMesh::computeAABB() {
	double vMin[3], vMax[3];
	for(U32 i=0; i < countNodes(); i++) {
		NODE n = const_nodeAt(i);
		vec3d p = n.pos;

		if(i == 0) {
			vMin[0] = vMax[0] = p[0];
			vMin[1] = vMax[1] = p[1];
			vMin[2] = vMax[2] = p[2];
		}
		else {
			vMin[0] = MATHMIN(vMin[0], p[0]);
			vMin[1] = MATHMIN(vMin[1], p[1]);
			vMin[2] = MATHMIN(vMin[2], p[2]);

			vMax[0] = MATHMAX(vMax[0], p[0]);
			vMax[1] = MATHMAX(vMax[1], p[1]);
			vMax[2] = MATHMAX(vMax[2], p[2]);
		}
	}

	//set AABB
	m_aabb.set(vec3f((float)vMin[0], (float)vMin[1], (float)vMin[2]),
			   vec3f((float)vMax[0], (float)vMax[1], (float)vMax[2]));
	return m_aabb;
}


