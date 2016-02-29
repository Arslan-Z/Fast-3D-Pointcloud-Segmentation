/*
 * Clustering.h
 *
 *  Created on: 19/05/2015
 *      Author: Francesco Verdoja <verdoja@di.unito.it>
 *
 *
 * Software License Agreement (BSD License)
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef CLUSTERING_H_
#define CLUSTERING_H_

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/supervoxel_clustering.h>
#include <math.h>

#include "ColorUtilities.h"
#include "ClusteringState.h"
#include "Testing.h"
#include "Object.h"

using namespace pcl;
using namespace std;

typedef PointXYZRGBA PointT;
typedef PointXYZL PointLT;
typedef PointXYZRGBL PointLCT;
typedef Supervoxel<PointT> SupervoxelT;
typedef std::map<uint32_t, SupervoxelT::Ptr> ClusteringT;
typedef std::multimap<uint32_t, uint32_t> AdjacencyMapT;
typedef std::multiset<float> DeltasDistribT;

enum ColorDistance {
	LAB_CIEDE00, RGB_EUCL
};
enum GeometricDistance {
	NORMALS_DIFF, CONVEX_NORMALS_DIFF
};
enum MergingCriterion {
	MANUAL_LAMBDA, ADAPTIVE_LAMBDA, EQUALIZATION
};

struct edge {
	uint32_t node_a, node_b;
	float distance;
};

class Clustering {
	ColorDistance delta_c_type;
	GeometricDistance delta_g_type;
	MergingCriterion merging_type;
	float lambda;
	short bins_num;
	std::map<short, float> cdf_c, cdf_g;
	bool set_initial_state, init_initial_weights;
	ClusteringState initial_state, state;

	bool is_convex(Normal norm1, PointT centroid1, Normal norm2,
			PointT centroid2) const;
	float normals_diff(Normal norm1, PointT centroid1, Normal norm2,
			PointT centroid2) const;
	std::pair<float, float> delta_c_g(SupervoxelT::Ptr supvox1,
			SupervoxelT::Ptr supvox2) const;
	float delta(SupervoxelT::Ptr supvox1, SupervoxelT::Ptr supvox2) const;
	AdjacencyMapT weight2adj(WeightMapT w_map) const;
	WeightMapT adj2weight(ClusteringT segm, AdjacencyMapT adj_map) const;
	void init_weights();
	void init_merging_parameters(DeltasDistribT deltas_c,
			DeltasDistribT deltas_g);
	std::map<short, float> compute_cdf(DeltasDistribT dist);
	float t_c(float delta_c) const;
	float t_g(float delta_g) const;
	void cluster(ClusteringState start, float threshold);
	//void merge(std::pair<uint32_t, uint32_t> supvox_ids);

	static void clear_adjacency(AdjacencyMapT * adjacency);
	static bool contains(WeightMapT w, uint32_t i1, uint32_t i2);
	static float deltas_mean(DeltasDistribT deltas);

public:
	Clustering();
	Clustering(ColorDistance c, GeometricDistance g, MergingCriterion m);

	// ALEX CODE
	// adds one supervoxel to the obj_number object of the map
	static void addSupervoxelToObject(uint32_t obj_number,
			pair<uint32_t, Supervoxel<PointT>::Ptr> supervoxel,
			map<uint32_t, Object*> &objects_set);
	static int findSupervoxelFromObject(uint32_t obj_number,
			uint32_t supervoxel_label, map<uint32_t, Object*> objects_set);
	static int removeSupervoxelFromObject(uint32_t obj_number,
			uint32_t supervoxel_label, map<uint32_t, Object*> &objects_set);
	static bool moveSupervoxelFromToObject(uint32_t obj_from, uint32_t obj_to,
			uint32_t supervoxel_label, map<uint32_t, Object*> &objects_set);
	static int getObjectFromSupervoxelLabel(uint32_t supervoxel_label,
			map<uint32_t, Object*> objects_set);
	static void computeDisconnectedGraphs(int obj_index,
			std::multimap<uint32_t, uint32_t> adjacency,
			map<uint32_t, Object*> &objects_set);
	static void computeAdjacencies(list<uint32_t> &together,
			multimap<uint32_t, uint32_t> adjacency,
			map<uint32_t, Supervoxel<PointT>::Ptr> supervoxel_set,
			map<uint32_t, Object*> objects_set);
	static map<uint32_t, Supervoxel<PointT>::Ptr> getGraphSupervoxels(
			multimap<uint32_t, uint32_t> adjacency,
			map<uint32_t, Supervoxel<PointT>::Ptr> supervoxel_set,
			map<uint32_t, Supervoxel<PointT>::Ptr> &graph_supervoxel);
	static void cutAdjacencies(uint32_t label,
			multimap<uint32_t, uint32_t>& adjacency);
	static void cutAdjacencies(uint32_t label, float max_distance,
			set<uint32_t>& visited, multimap<uint32_t, uint32_t>& adjacency,
			map<uint32_t, Supervoxel<PointT>::Ptr> supervoxel_set);
	static void edgeCutter(multimap<uint32_t, uint32_t>& adjacency,
			 map<uint32_t, Object*> objects_set, float toll_multiplier);
	static bool compare_edge (const edge& first, const edge& second);
	void mergeSupervoxel(std::pair<uint32_t, uint32_t> supvox_ids);
	ClusteringState getState();
	void merge(std::pair<uint32_t, uint32_t> supvox_ids);
	std::map<float, performanceSet> all_thresh_v2(
		Clustering segmentationBackup
	/*ClusteringT supervoxel_clusters, AdjacencyMapT label_adjacency*/,
	PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
	float end_thresh, float step_thresh, float toll_multiplier, bool CVX, bool GA);
	void analyze_graph(Clustering& segmentation,
			/*multimap<uint32_t, uint32_t>& adjacency,*/ float toll_multiplier);
	std::pair<float, performanceSet> all_thresh_v2_internal(
			Clustering segmentation, PointCloud<PointLT>::Ptr ground_truth,
			float thresh, float toll_multiplier, bool GA);
	std::map<float, performanceSet> all_thresh_v3(
			ClusteringT supervoxel_clusters, AdjacencyMapT label_adjacency,
			PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
			float end_thresh, float step_thresh, float toll_multiplier,
			bool CVX, bool GA);
	// END ALEX

	void set_delta_c(ColorDistance d) {
		delta_c_type = d;
	}
	void set_delta_g(GeometricDistance d) {
		delta_g_type = d;
	}
	void set_merging(MergingCriterion m);
	void set_lambda(float l);
	void set_bins_num(short b);
	void set_initialstate(ClusteringT segm, AdjacencyMapT adj);

	ColorDistance get_delta_c() const {
		return delta_c_type;
	}
	GeometricDistance get_delta_g() const {
		return delta_g_type;
	}
	MergingCriterion get_merging() const {
		return merging_type;
	}
	float get_lambda() const {
		return lambda;
	}
	short get_bins_num() const {
		return bins_num;
	}
	std::pair<ClusteringT, AdjacencyMapT> get_currentstate() const;

	PointCloud<PointT>::Ptr get_colored_cloud() const;
	PointCloud<PointXYZL>::Ptr get_labeled_cloud() const;

	void cluster(float threshold);

	/*std::map<float, performanceSet> all_thresh(
			PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
			float end_thresh, float step_thresh);*/
	std::map<float, performanceSet> all_thresh(
			PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
			float end_thresh, float step_thresh);
	std::pair<float, performanceSet> best_thresh(
			PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
			float end_thresh, float step_thresh);
	std::pair<float, performanceSet> best_thresh(
			std::map<float, performanceSet> all_thresh);

	void test_all() const;

	static PointCloud<PointT>::Ptr label2color(
			PointCloud<PointLT>::Ptr label_cloud);
	static PointCloud<PointLT>::Ptr color2label(
			PointCloud<PointT>::Ptr colored_cloud);
};

#endif /* CLUSTERING_H_ */
