#pragma once

#include <any>
#include <memory>
#include <random>
#include <boost/shared_ptr.hpp>
#include <glim/backend/global_mapping_base.hpp>

namespace gtsam {
class Values;
class NonlinearFactorGraph;
}  // namespace gtsam

namespace gtsam_ext {
class ISAM2Ext;
class StreamTempBufferRoundRobin;
}  // namespace gtsam_ext

namespace glim {

class IMUIntegration;

struct GlobalMappingParams {
public:
  GlobalMappingParams();
  ~GlobalMappingParams();

public:
  bool enable_gpu;
  bool enable_imu;
  bool enable_between_factors;
  std::string between_registration_type;

  std::string registration_error_factor_type;
  double submap_voxel_resolution;
  double randomsampling_rate;
  double max_implicit_loop_distance;
  double min_implicit_loop_overlap;

  bool use_isam2_dogleg;
  double isam2_relinearize_skip;
  double isam2_relinearize_thresh;
};

class GlobalMapping : public GlobalMappingBase {
public:
  GlobalMapping(const GlobalMappingParams& params = GlobalMappingParams());
  virtual ~GlobalMapping();

  virtual void insert_imu(const double stamp, const Eigen::Vector3d& linear_acc, const Eigen::Vector3d& angular_vel) override;
  virtual void insert_submap(const SubMap::Ptr& submap) override;

  virtual void optimize() override;

  virtual void save(const std::string& path) override;
  virtual std::vector<Eigen::Vector4d, Eigen::aligned_allocator<Eigen::Vector4d>> export_points() override;

  bool load(const std::string& path);

private:
  void insert_submap(int current, const SubMap::Ptr& submap);

  boost::shared_ptr<gtsam::NonlinearFactorGraph> create_between_factors(int current) const;
  boost::shared_ptr<gtsam::NonlinearFactorGraph> create_matching_cost_factors(int current) const;

  void update_submaps();

private:
  using Params = GlobalMappingParams;
  Params params;

  std::mt19937 mt;

  std::unique_ptr<IMUIntegration> imu_integration;
  std::any stream_buffer_roundrobin;

  std::vector<SubMap::Ptr> submaps;
  std::vector<gtsam_ext::Frame::Ptr> subsampled_submaps;
  std::vector<gtsam_ext::VoxelizedFrame::Ptr> voxelized_submaps;

  std::unique_ptr<gtsam::Values> new_values;
  std::unique_ptr<gtsam::NonlinearFactorGraph> new_factors;

  std::unique_ptr<gtsam_ext::ISAM2Ext> isam2;
};
}  // namespace glim