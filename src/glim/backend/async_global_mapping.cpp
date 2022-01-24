#include <glim/backend/async_global_mapping.hpp>

#include <glim/backend/callbacks.hpp>

namespace glim {

AsyncGlobalMapping::AsyncGlobalMapping(const std::shared_ptr<glim::GlobalMappingBase>& global_mapping) : global_mapping(global_mapping) {
  saving = false;
  request_to_optimize = false;
  GlobalMappingCallbacks::request_to_optimize.add([this] { request_to_optimize = true; });

  kill_switch = false;
  end_of_sequence = false;
  thread = std::thread([this] { run(); });
}

AsyncGlobalMapping::~AsyncGlobalMapping() {
  kill_switch = true;
  join();
}

void AsyncGlobalMapping::insert_image(const double stamp, const cv::Mat& image) {
  input_image_queue.push_back(std::make_pair(stamp, image));
}

void AsyncGlobalMapping::insert_imu(const double stamp, const Eigen::Vector3d& linear_acc, const Eigen::Vector3d& angular_vel) {
  Eigen::Matrix<double, 7, 1> imu_data;
  imu_data << stamp, linear_acc, angular_vel;
  input_imu_queue.push_back(imu_data);
}

void AsyncGlobalMapping::insert_submap(const SubMap::Ptr& submap) {
  input_submap_queue.push_back(submap);
}

void AsyncGlobalMapping::join() {
  end_of_sequence = true;
  if (thread.joinable()) {
    thread.join();
  }
}

int AsyncGlobalMapping::input_queue_size() const {
  return input_submap_queue.size();
}

int AsyncGlobalMapping::output_queue_size() const {
  return 0;
}

void AsyncGlobalMapping::save(const std::string& path) {
  std::cout << "saving to " << path << "..." << std::endl;
  saving = true;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  global_mapping->save(path);
  saving = false;
  std::cout << "saved" << std::endl;
}

std::vector<Eigen::Vector4d, Eigen::aligned_allocator<Eigen::Vector4d>> AsyncGlobalMapping::export_points() {
  saving = true;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto points = global_mapping->export_points();
  saving = false;

  return points;
}

void AsyncGlobalMapping::run() {
  auto last_optimization_time = std::chrono::high_resolution_clock::now();

  while (!kill_switch) {
    while (saving) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto images = input_image_queue.get_all_and_clear();
    auto imu_frames = input_imu_queue.get_all_and_clear();
    auto submaps = input_submap_queue.get_all_and_clear();

    if (images.empty() && imu_frames.empty() && submaps.empty()) {
      if (end_of_sequence) {
        break;
      }

      if (request_to_optimize || std::chrono::high_resolution_clock::now() - last_optimization_time > std::chrono::seconds(5)) {
        request_to_optimize = false;
        global_mapping->optimize();
        last_optimization_time = std::chrono::high_resolution_clock::now();
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    for (const auto& image : images) {
      global_mapping->insert_image(image.first, image.second);
    }

    for (const auto& imu : imu_frames) {
      const double stamp = imu[0];
      const Eigen::Vector3d linear_acc = imu.block<3, 1>(1, 0);
      const Eigen::Vector3d angular_vel = imu.block<3, 1>(4, 0);
      global_mapping->insert_imu(stamp, linear_acc, angular_vel);
    }

    for (const auto& submap : submaps) {
      global_mapping->insert_submap(submap);
    }

    last_optimization_time = std::chrono::high_resolution_clock::now();
  }
}

}  // namespace glim