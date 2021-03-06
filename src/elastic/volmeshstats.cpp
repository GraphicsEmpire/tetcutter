/*
 * VolMeshStats.cpp
 *
 *  Created on: Sep 15, 2014
 *      Author: pourya
 */

#include "elastic/volmeshstats.h"

namespace ps {
namespace elastic {

VolMeshStats::VolMeshStats() {
	// TODO Auto-generated constructor stub

}

VolMeshStats::~VolMeshStats() {
	// TODO Auto-generated destructor stub
}

void VolMeshStats::printAllStats(const VolMesh* pmesh) {

	double volMin = 0.0;
	double volMax = 0.0;
	double edgeLenMin = 0.0;
	double edgeLenMax = 0.0;
	double minAR = 0.0;
	printf("===========================begin mesh stats============================\n");

	//compute
	assert(computeVolMaxMin(pmesh, volMax, volMin));
	assert(computeEdgeLenMaxMin(pmesh, edgeLenMax, edgeLenMin));
	assert(computeMinAspectRatio(pmesh, minAR));
	double vMaxFvMin = (volMin == 0.0) ? volMax : (volMax/volMin);
	double edgeMaxFedgeMin = (edgeLenMin == 0.0) ? edgeLenMax : (edgeLenMax/edgeLenMin);

	//print
	printf("INFO: Vol Max: %.8f, Min: %.8f, max/min: %.8f \n", volMax, volMin, vMaxFvMin);
	printf("INFO: EdgeLen Max: %.8f, Min: %.8f, max/min: %.8f \n", edgeLenMax, edgeLenMin, edgeMaxFedgeMin);
	printf("INFO: minAspectRatio: %.8f\n", minAR);
	printf("============================end mesh stats=============================\n");
}

bool VolMeshStats::computeVolMaxMin(const VolMesh* pmesh, double& outVolMax, double& outVolMin) {
	if(pmesh == NULL)
		return false;

	outVolMax = GetMinLimit<double>();
	outVolMin = GetMaxLimit<double>();
	for(U32 i=0; i < pmesh->countCells(); i++) {
		double v = pmesh->computeCellVolume(i);

		if(v > FLAT_CELL_VOLUME) {
			outVolMax = MATHMAX(v, outVolMax);
			outVolMin = MATHMIN(v, outVolMin);
		}
	}

	return true;
}

bool VolMeshStats::computeEdgeLenMaxMin(const VolMesh* pmesh, double& outEdgeLenMax, double& outEdgeLenMin) {
	if(pmesh == NULL)
		return false;

	outEdgeLenMax = GetMinLimit<double>();
	outEdgeLenMin = GetMaxLimit<double>();
	for(U32 i=0; i < pmesh->countEdges(); i++) {
		const ps::elastic::EDGE& edge = pmesh->const_edgeAt(i);
		vec3d s0 = pmesh->const_nodeAt(edge.from).pos;
		vec3d s1 = pmesh->const_nodeAt(edge.to).pos;
		double d = vec3d::distance(s0, s1);

		if(d > MIN_EDGE_LENGTH) {
			outEdgeLenMax = MATHMAX(d, outEdgeLenMax);
			outEdgeLenMin = MATHMIN(d, outEdgeLenMin);
		}
	}

	return true;
}

bool VolMeshStats::computeMinAspectRatio(const VolMesh* pmesh, double& outMinAR) {
	if(pmesh == NULL)
		return false;

	outMinAR = GetMaxLimit<double>();
	for(U32 i=0; i < pmesh->countCells(); i++) {
		double ar = pmesh->computeAspectRatio(i);
		outMinAR = MATHMIN(outMinAR, ar);
	}

	return true;

}

} /* namespace MESH */
} /* namespace PS */
