/*
 * Clustering.cpp
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

#include "Clustering.h"

float Clustering::normals_diff(Normal norm1, PointT centroid1, Normal norm2,
		PointT centroid2) const {
	Eigen::Vector3f N1 = norm1.getNormalVector3fMap();
	Eigen::Vector3f C1 = centroid1.getVector3fMap();
	Eigen::Vector3f N2 = norm2.getNormalVector3fMap();
	Eigen::Vector3f C2 = centroid2.getVector3fMap();

	Eigen::Vector3f C = C2 - C1;
	C /= C.norm();

	float N1xN2 = N1.cross(N2).norm();
	float N1_C = std::abs(N1.dot(C));
	float N2_C = std::abs(N2.dot(C));

	float delta_g = (N1xN2 + N1_C + N2_C) / 3;

//	std::cout << "c1 =\n" << C1 << "\n";
//	std::cout << "c2 =\n" << C2 << "\n";
//	std::cout << "C =\n" << C << "\n";
//	std::cout << "N1_C =\n" << N1_C << "\n";
//	std::cout << "N2_C =\n" << N2_C << "\n";
//	std::cout <<"n1 =\n"<< N1 << "\n";
//	std::cout <<"n2 =\n"<< N2 << "\n";
//	std::cout <<"n1 x n2 =\n"<< N1.cross(N2) << "\n|n1 x n2| = " << N1xN2 << "\n";

	return delta_g;
}

std::pair<float, float> Clustering::delta_c_g(SupervoxelT::Ptr supvox1,
		SupervoxelT::Ptr supvox2) const {
	float delta_c = 0;
	float * rgb1 = ColorUtilities::mean_color(supvox1);
	float * rgb2 = ColorUtilities::mean_color(supvox2);
	switch (delta_c_type) {
	case LAB_CIEDE00:
		float *lab1, *lab2;
		lab1 = ColorUtilities::rgb2lab(rgb1);
		lab2 = ColorUtilities::rgb2lab(rgb2);
		delta_c = ColorUtilities::lab_ciede00(lab1, lab2);
		delta_c /= LAB_RANGE;
		break;
	case RGB_EUCL:
		delta_c = ColorUtilities::rgb_eucl(rgb1, rgb2);
		delta_c /= RGB_RANGE;
	}

	float delta_g = 0;
	switch (delta_g_type) {
	case NORMALS_DIFF:
		Normal n1 = supvox1->normal_;
		Normal n2 = supvox2->normal_;
		PointT c1 = supvox1->centroid_;
		PointT c2 = supvox2->centroid_;
		delta_g = normals_diff(n1, c1, n2, c2);
	}

	std::pair<float, float> ret(delta_c, delta_g);
	return ret;
}

float Clustering::delta(SupervoxelT::Ptr supvox1,
		SupervoxelT::Ptr supvox2) const {

	std::pair<float, float> deltas = delta_c_g(supvox1, supvox2);

//	printf("delta_c = %f | delta_g = %f\n", delta_c, delta_g);

	float delta = t_c(deltas.first) + t_g(deltas.second);
	return delta;
}

AdjacencyMapT Clustering::weight2adj(WeightMapT w_map) const {
	AdjacencyMapT adj_map;

	WeightMapT::iterator it = w_map.begin();
	WeightMapT::iterator it_end = w_map.end();
	for (; it != it_end; ++it) {
		adj_map.insert(it->second);
	}

	return adj_map;
}

WeightMapT Clustering::adj2weight(ClusteringT segm,
		AdjacencyMapT adj_map) const {
	WeightMapT w_map;

	AdjacencyMapT::iterator it = adj_map.begin();
	AdjacencyMapT::iterator it_end = adj_map.end();
	for (; it != it_end; ++it) {
		WeightedPairT elem;
		elem.first = -1;
		elem.second = *(it);
		w_map.insert(elem);
	}

	return w_map;
}

void Clustering::init_weights() {
	std::map<std::string, std::pair<float, float> > temp_deltas;
	DeltasDistribT deltas_c;
	DeltasDistribT deltas_g;
	WeightMapT w_new;

	WeightMapT::iterator it = initial_state.weight_map.begin();
	WeightMapT::iterator it_end = initial_state.weight_map.end();
	for (; it != it_end; ++it) {
		uint32_t sup1_id = it->second.first;
		uint32_t sup2_id = it->second.second;
		std::stringstream ids;
		ids << sup1_id << "-" << sup2_id;
		SupervoxelT::Ptr sup1 = initial_state.segments.at(sup1_id);
		SupervoxelT::Ptr sup2 = initial_state.segments.at(sup2_id);
		std::pair<float, float> deltas = delta_c_g(sup1, sup2);
		temp_deltas.insert(
				std::pair<std::string, std::pair<float, float> >(ids.str(),
						deltas));
		deltas_c.insert(deltas.first);
		deltas_g.insert(deltas.second);
	}

	init_merging_parameters(deltas_c, deltas_g);

	it = initial_state.weight_map.begin();
	for (; it != it_end; ++it) {
		uint32_t sup1_id = it->second.first;
		uint32_t sup2_id = it->second.second;
		std::stringstream ids;
		ids << sup1_id << "-" << sup2_id;
		std::pair<float, float> deltas = temp_deltas.at(ids.str());
		float delta = t_c(deltas.first) + t_g(deltas.second);
		w_new.insert(WeightedPairT(delta, it->second));
	}

	initial_state.set_weight_map(w_new);

	init_initial_weights = true;
}

void Clustering::init_merging_parameters(DeltasDistribT deltas_c,
		DeltasDistribT deltas_g) {
	switch (merging_type) {
	case MANUAL_LAMBDA: {
		break;
	}
	case ADAPTIVE_LAMBDA: {
		float mean_c = deltas_mean(deltas_c);
		float mean_g = deltas_mean(deltas_g);
		lambda = mean_g / (mean_c + mean_g);
		break;
	}
	case EQUALIZATION: {
		cdf_c = compute_cdf(deltas_c);
		cdf_g = compute_cdf(deltas_g);
	}
	}
}

std::map<short, float> Clustering::compute_cdf(DeltasDistribT dist) {
	std::map<short, float> cdf;
	int bins[bins_num] = { };

	DeltasDistribT::iterator d_itr, d_itr_end;
	d_itr = dist.begin();
	d_itr_end = dist.end();
	int n = dist.size();
	for (; d_itr != d_itr_end; ++d_itr) {
		float d = *d_itr;
		short bin = std::floor(d * bins_num);
		if (bin == bins_num)
			bin--;
		bins[bin]++;
	}

	for (short i = 0; i < bins_num; i++) {
		float v = 0;
		for (short j = 0; j <= i; j++)
			v += bins[j];
		v /= n;
		cdf.insert(std::pair<short, float>(i, v));
	}

	return cdf;
}

float Clustering::t_c(float delta_c) const {
	float ret = 0;
	switch (merging_type) {
	case MANUAL_LAMBDA: {
		ret = lambda * delta_c;
		break;
	}
	case ADAPTIVE_LAMBDA: {
		ret = lambda * delta_c;
		break;
	}
	case EQUALIZATION: {
		short bin = std::floor(delta_c * bins_num);
		if (bin == bins_num)
			bin--;
		ret = cdf_c.at(bin) / 2;
	}
	}
	return ret;
}

float Clustering::t_g(float delta_g) const {
	float ret = 0;
	switch (merging_type) {
	case MANUAL_LAMBDA: {
		ret = (1 - lambda) * delta_g;
		break;
	}
	case ADAPTIVE_LAMBDA: {
		ret = (1 - lambda) * delta_g;
		break;
	}
	case EQUALIZATION: {
		short bin = std::floor(delta_g * bins_num);
		ret = cdf_g.at(bin) / 2;
	}
	}
	return ret;
}

void Clustering::cluster(ClusteringState start, float threshold) {
	state = start;

	WeightedPairT next;
	while (!state.weight_map.empty()
			&& (next = state.get_first_weight(), next.first < threshold)) {
		console::print_debug("left: %de/%dp - w: %f - [%d, %d]...",
				state.weight_map.size(), state.segments.size(), next.first,
				next.second.first, next.second.second);
		merge(next.second);
		console::print_debug("OK\n");
	}
}

void Clustering::merge(std::pair<uint32_t, uint32_t> supvox_ids) {
	SupervoxelT::Ptr sup1 = state.segments.at(supvox_ids.first);
	SupervoxelT::Ptr sup2 = state.segments.at(supvox_ids.second);
	SupervoxelT::Ptr sup_new = boost::make_shared<SupervoxelT>();

	*(sup_new->voxels_) = *(sup1->voxels_) + *(sup2->voxels_);
	*(sup_new->normals_) = *(sup1->normals_) + *(sup2->normals_);

	PointT new_centr;
	computeCentroid(*(sup_new->voxels_), new_centr);
	sup_new->centroid_ = new_centr;

	Eigen::Vector4f new_norm;
	float new_curv;
	computePointNormal(*(sup_new->voxels_), new_norm, new_curv);
	flipNormalTowardsViewpoint(sup_new->centroid_, 0, 0, 0, new_norm);
	new_norm[3] = 0.0f;
	new_norm.normalize();
	sup_new->normal_.normal_x = new_norm[0];
	sup_new->normal_.normal_y = new_norm[1];
	sup_new->normal_.normal_z = new_norm[2];
	sup_new->normal_.curvature = new_curv;

	state.segments.erase(supvox_ids.first);
	state.segments.erase(supvox_ids.second);
	state.segments.insert(
			std::pair<uint32_t, SupervoxelT::Ptr>(supvox_ids.first, sup_new));

	WeightMapT new_map;

	WeightMapT::iterator it = state.weight_map.begin();
	++it;
	WeightMapT::iterator it_end = state.weight_map.end();
	for (; it != it_end; ++it) {
		std::pair<uint32_t, uint32_t> curr_ids = it->second;
		if (curr_ids.first == supvox_ids.first
				|| curr_ids.second == supvox_ids.first) {
			if (!contains(new_map, curr_ids.first, curr_ids.second)) {
				float w = delta(state.segments.at(curr_ids.first),
						state.segments.at(curr_ids.second));
				new_map.insert(WeightedPairT(w, curr_ids));
			}
		} else if (curr_ids.first == supvox_ids.second) {
			curr_ids.first = supvox_ids.first;
			if (!contains(new_map, curr_ids.first, curr_ids.second)) {
				float w = delta(state.segments.at(curr_ids.first),
						state.segments.at(curr_ids.second));
				new_map.insert(WeightedPairT(w, curr_ids));
			}
		} else if (curr_ids.second == supvox_ids.second) {
			if (curr_ids.first < supvox_ids.first)
				curr_ids.second = supvox_ids.first;
			else {
				curr_ids.second = curr_ids.first;
				curr_ids.first = supvox_ids.first;
			}
			if (!contains(new_map, curr_ids.first, curr_ids.second)) {
				float w = delta(state.segments.at(curr_ids.first),
						state.segments.at(curr_ids.second));
				new_map.insert(WeightedPairT(w, curr_ids));
			}
		} else {
			new_map.insert(*it);
		}
	}
	state.weight_map = new_map;
}

void Clustering::clear_adjacency(AdjacencyMapT * adjacency) {
	AdjacencyMapT::iterator it = adjacency->begin();
	AdjacencyMapT::iterator it_end = adjacency->end();
	while (it != it_end) {
		if (it->first > it->second) {
			adjacency->erase(it++);
		} else {
			++it;
		}
	}
//	for (; it != it_end; ++it) {
//		uint32_t key = it->first;
//		uint32_t value = it->second;
//		std::pair<AdjacencyMapT::iterator, AdjacencyMapT::iterator> range =
//				adjacency->equal_range(value);
//		AdjacencyMapT::iterator val_it = range.first;
//		for (; val_it != range.second; ++val_it) {
//			if (val_it->second == key) {
//				adjacency->erase(val_it);
//				break;
//			}
//		}
//	}
}

bool Clustering::contains(WeightMapT w, uint32_t i1, uint32_t i2) {
	WeightMapT::iterator it = w.begin();
	WeightMapT::iterator it_end = w.end();
	for (; it != it_end; ++it) {
		std::pair<uint32_t, uint32_t> ids = it->second;
		if (ids.first == i1 && ids.second == i2)
			return true;
	}
	return false;
}

float Clustering::deltas_mean(DeltasDistribT deltas) {
	DeltasDistribT::iterator d_itr, d_itr_end;
	d_itr = deltas.begin();
	d_itr_end = deltas.end();
	float count = 0;
	float mean_d = 0;
	for (; d_itr != d_itr_end; ++d_itr) {
		float delta = *d_itr;
		count++;

		mean_d = mean_d + (1 / count) * (delta - mean_d);
	}
	return mean_d;
}

Clustering::Clustering() {
	set_delta_c(LAB_CIEDE00);
	set_delta_g(NORMALS_DIFF);
	set_merging(ADAPTIVE_LAMBDA);
	set_initial_state = false;
	init_initial_weights = false;
}

Clustering::Clustering(ColorDistance c, GeometricDistance g,
		MergingCriterion m) {
	set_delta_c(c);
	set_delta_g(g);
	set_merging(m);
	set_initial_state = false;
	init_initial_weights = false;
}

void Clustering::set_merging(MergingCriterion m) {
	merging_type = m;
	lambda = 0.5;
	bins_num = 500;
	init_initial_weights = false;
}

void Clustering::set_lambda(float l) {
	if (merging_type != MANUAL_LAMBDA)
		throw std::logic_error(
				"Lambda can be set only if the merging criterion is set to MANUAL_LAMBDA");
	if (l < 0 || l > 1)
		throw std::invalid_argument("Argument outside range [0, 1]");
	lambda = l;
	init_initial_weights = false;
}

void Clustering::set_bins_num(short b) {
	if (merging_type != EQUALIZATION)
		throw std::logic_error(
				"Bins number can be set only if the merging criterion is set to EQUALIZATION");
	if (b < 0)
		throw std::invalid_argument("Argument lower than 0");
	bins_num = b;
	init_initial_weights = false;
}

void Clustering::set_initialstate(ClusteringT segm, AdjacencyMapT adj) {
	clear_adjacency(&adj);
	ClusteringState init_state(segm, adj2weight(segm, adj));
	initial_state = init_state;
	state = init_state;
	set_initial_state = true;
	init_initial_weights = false;
}

std::pair<ClusteringT, AdjacencyMapT> Clustering::get_currentstate() const {
	std::pair<ClusteringT, AdjacencyMapT> ret;
	ret.first = state.segments;
	ret.second = weight2adj(state.weight_map);
	return ret;
}

PointCloud<PointT>::Ptr Clustering::get_colored_cloud() const {
	return label2color(get_labeled_cloud());
}

PointCloud<PointXYZL>::Ptr Clustering::get_labeled_cloud() const {
	PointCloud<PointXYZL>::Ptr label_cloud(new PointCloud<PointXYZL>);

	ClusteringT::const_iterator it = state.segments.begin();
	ClusteringT::const_iterator it_end = state.segments.end();

	uint32_t current_l = 0;
	for (; it != it_end; ++it) {
		PointCloud<PointT> cloud = *(it->second->voxels_);
		PointCloud<PointT>::iterator it_cloud = cloud.begin();
		PointCloud<PointT>::iterator it_cloud_end = cloud.end();
		for (; it_cloud != it_cloud_end; ++it_cloud) {
			PointXYZL p;
			p.x = it_cloud->x;
			p.y = it_cloud->y;
			p.z = it_cloud->z;
			p.label = current_l;
			label_cloud->push_back(p);
		}
		current_l++;
	}

	return label_cloud;
}

void Clustering::cluster(float threshold) {
	if (!set_initial_state)
		throw std::logic_error("Cannot call 'cluster' before "
				"setting an initial state with 'set_initialstate'");

	if (!init_initial_weights)
		init_weights();

	cluster(initial_state, threshold);
}
std::map<float, performanceSet> Clustering::all_thresh(
		PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
		float end_thresh, float step_thresh) {
	if (start_thresh < 0 || start_thresh > 1 || end_thresh < 0 || end_thresh > 1
			|| step_thresh < 0 || step_thresh > 1) {
		throw std::out_of_range(
				"start_thresh, end_thresh and/or step_thresh outside of range [0, 1]");
	}
	if (start_thresh > end_thresh) {
		console::print_warn(
				"Start threshold greater then end threshold, inverting.\n");
		float temp = end_thresh;
		end_thresh = start_thresh;
		start_thresh = temp;
	}

	console::print_info("Testing thresholds from %f to %f (step %f)\n",
			start_thresh, end_thresh, step_thresh);

	std::map<float, performanceSet> thresholds;
	cluster(start_thresh);
	Testing test(get_labeled_cloud(), ground_truth);
	performanceSet p = test.eval_performance();
	thresholds.insert(std::pair<float, performanceSet>(start_thresh, p));
	console::print_info("<T, Fscore, voi, wov> = <%f, %f, %f, %f>\n",
			start_thresh, p.fscore, p.voi, p.wov);

	for (float t = start_thresh + step_thresh; t <= end_thresh; t +=
			step_thresh) {
		cluster(state, t);
		test.set_segm(get_labeled_cloud());
		p = test.eval_performance();
		thresholds.insert(std::pair<float, performanceSet>(t, p));
		console::print_info("<T, Fscore, voi, wov> = <%f, %f, %f, %f>\n", t,
				p.fscore, p.voi, p.wov);
	}

	return thresholds;
}

std::pair<float, performanceSet> Clustering::best_thresh(
		PointCloud<PointLT>::Ptr ground_truth, float start_thresh,
		float end_thresh, float step_thresh) {
	std::map<float, performanceSet> thresholds = all_thresh(ground_truth,
			start_thresh, end_thresh, step_thresh);
	return best_thresh(thresholds);
}

std::pair<float, performanceSet> Clustering::best_thresh(
		std::map<float, performanceSet> all_thresh) {
	float best_t = 0;
	performanceSet best_performance;

	std::map<float, performanceSet>::iterator it = all_thresh.begin();

	for (; it != all_thresh.end(); ++it) {
		if (it->second.fscore > best_performance.fscore) {
			best_performance = it->second;
			best_t = it->first;
		}
	}

	return std::pair<float, performanceSet>(best_t, best_performance);
}

void Clustering::test_all() const {
	ColorUtilities::rgb_test();
	ColorUtilities::lab_test();
	ColorUtilities::convert_test();

//	DeltasDistribT d;
//	for(int i = 0; i<10; i++)
//		d.insert(i);
//	printf("*** Mean deltas test ***\nMean = %f\n", deltas_mean(d));
}

PointCloud<PointT>::Ptr Clustering::label2color(
		PointCloud<PointLT>::Ptr label_cloud) {
	PointCloud<PointT>::Ptr colored_cloud(new PointCloud<PointT>);
	PointCloud<PointLCT>::Ptr temp_cloud(new PointCloud<PointLCT>);

	copyPointCloud(*label_cloud, *temp_cloud);

	PointCloud<PointLCT>::iterator it = temp_cloud->begin();
	PointCloud<PointLCT>::iterator it_end = temp_cloud->end();

	for (; it != it_end; ++it) {
		uint8_t * rgb = ColorUtilities::get_glasbey(it->label);
		it->r = rgb[0];
		it->g = rgb[1];
		it->b = rgb[2];
	}

	copyPointCloud(*temp_cloud, *colored_cloud);
	return colored_cloud;
}

PointCloud<PointLT>::Ptr Clustering::color2label(
		PointCloud<PointT>::Ptr colored_cloud) {
	PointCloud<PointLT>::Ptr label_cloud(new PointCloud<PointLT>);
	PointCloud<PointLCT>::Ptr temp_cloud(new PointCloud<PointLCT>);
	std::map<float, uint32_t> mappings;
	copyPointCloud(*colored_cloud, *temp_cloud);

	PointCloud<PointLCT>::iterator it = temp_cloud->begin();
	PointCloud<PointLCT>::iterator it_end = temp_cloud->end();

	uint32_t i = 0;
	for (; it != it_end; ++it) {
		if (mappings.count(it->rgb) != 0)
			it->label = mappings.at(it->rgb);
		else {
			it->label = i;
			mappings.insert(std::pair<float, uint32_t>(it->rgb, i));
			i++;
		}
	}

	copyPointCloud(*temp_cloud, *label_cloud);
	return label_cloud;
}
