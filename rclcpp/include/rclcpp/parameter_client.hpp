// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RCLCPP_RCLCPP_PARAMETER_CLIENT_HPP_
#define RCLCPP_RCLCPP_PARAMETER_CLIENT_HPP_

#include <string>

#include <rmw/rmw.h>

#include <rclcpp/executors.hpp>
#include <rclcpp/macros.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/parameter.hpp>

#include <rcl_interfaces/GetParameters.h>
#include <rcl_interfaces/GetParameterTypes.h>
#include <rcl_interfaces/Parameter.h>
#include <rcl_interfaces/ParameterDescriptor.h>
#include <rcl_interfaces/ParameterType.h>
#include <rcl_interfaces/SetParameters.h>
#include <rcl_interfaces/SetParametersAtomically.h>
#include <rcl_interfaces/ListParameters.h>
#include <rcl_interfaces/DescribeParameters.h>

namespace rclcpp
{

namespace parameter_client
{

class AsyncParametersClient
{

public:
  RCLCPP_MAKE_SHARED_DEFINITIONS(AsyncParametersClient);

  AsyncParametersClient(const rclcpp::node::Node::SharedPtr & node,
    const std::string & remote_node_name = "")
  : node_(node)
  {
    if (remote_node_name != "") {
      remote_node_name_ = remote_node_name;
    } else {
      remote_node_name_ = node_->get_name();
    }
    get_parameters_client_ = node_->create_client<rcl_interfaces::GetParameters>(
      remote_node_name_ + "__get_parameters");
    get_parameter_types_client_ = node_->create_client<rcl_interfaces::GetParameterTypes>(
      remote_node_name_ + "__get_parameter_types");
    set_parameters_client_ = node_->create_client<rcl_interfaces::SetParameters>(
      remote_node_name_ + "__set_parameters");
    list_parameters_client_ = node_->create_client<rcl_interfaces::ListParameters>(
      remote_node_name_ + "__list_parameters");
    describe_parameters_client_ = node_->create_client<rcl_interfaces::DescribeParameters>(
      remote_node_name_ + "__describe_parameters");
  }

  std::shared_future<std::vector<rclcpp::parameter::ParameterVariant>>
  get_parameters(
    std::vector<std::string> names,
    std::function<void(
      std::shared_future<std::vector<rclcpp::parameter::ParameterVariant>>)> callback = nullptr)
  {
    std::promise<std::vector<rclcpp::parameter::ParameterVariant>> promise_result;
    auto future_result = promise_result.get_future().share();

    auto request = std::make_shared<rcl_interfaces::GetParameters::Request>();
    request->names = names;

    get_parameters_client_->async_send_request(
      request,
      [&names, &promise_result, &future_result, &callback](
        rclcpp::client::Client<rcl_interfaces::GetParameters>::SharedFuture cb_f) {
          std::vector<rclcpp::parameter::ParameterVariant> parameter_variants;
          auto & pvalues = cb_f.get()->values;

          std::transform(pvalues.begin(), pvalues.end(), std::back_inserter(parameter_variants),
          [&names, &pvalues](rcl_interfaces::ParameterValue pvalue) {
            auto i = &pvalue - &pvalues[0];
            rcl_interfaces::Parameter parameter;
            parameter.name = names[i];
            parameter.value = pvalue;
            return rclcpp::parameter::ParameterVariant::from_parameter(parameter);
          });
          promise_result.set_value(parameter_variants);
          if (callback != nullptr) {
            callback(future_result);
          }
        }
      );

    return future_result;
  }

  std::shared_future<std::vector<rclcpp::parameter::ParameterType>>
  get_parameter_types(
    std::vector<std::string> parameter_names,
    std::function<void(
      std::shared_future<std::vector<rclcpp::parameter::ParameterType>>)> callback = nullptr)
  {
    std::promise<std::vector<rclcpp::parameter::ParameterType>> promise_result;
    auto future_result = promise_result.get_future().share();

    auto request = std::make_shared<rcl_interfaces::GetParameterTypes::Request>();
    request->parameter_names = parameter_names;

    get_parameter_types_client_->async_send_request(
      request,
      [&promise_result, &future_result, &callback](
        rclcpp::client::Client<rcl_interfaces::GetParameterTypes>::SharedFuture cb_f) {
          std::vector<rclcpp::parameter::ParameterType> parameter_types;
          auto & pts = cb_f.get()->parameter_types;
          std::transform(pts.begin(), pts.end(), std::back_inserter(parameter_types),
          [](uint8_t pt) {return static_cast<rclcpp::parameter::ParameterType>(pt); });
          promise_result.set_value(parameter_types);
          if (callback != nullptr) {
            callback(future_result);
          }
        }
      );

    return future_result;
  }

  std::shared_future<std::vector<rcl_interfaces::SetParametersResult>>
  set_parameters(
    std::vector<rclcpp::parameter::ParameterVariant> parameters,
    std::function<void(
      std::shared_future<std::vector<rcl_interfaces::SetParametersResult>>)> callback = nullptr)
  {
    std::promise<std::vector<rcl_interfaces::SetParametersResult>> promise_result;
    auto future_result = promise_result.get_future().share();

    auto request = std::make_shared<rcl_interfaces::SetParameters::Request>();

    std::transform(parameters.begin(), parameters.end(), std::back_inserter(
        request->parameters), [](
        rclcpp::parameter::ParameterVariant p) {return p.to_parameter(); });

    set_parameters_client_->async_send_request(
      request,
      [&promise_result, &future_result, &callback](
        rclcpp::client::Client<rcl_interfaces::SetParameters>::SharedFuture cb_f) {
          promise_result.set_value(cb_f.get()->results);
          if (callback != nullptr) {
            callback(future_result);
          }
        }
      );

    return future_result;
  }

  std::shared_future<rcl_interfaces::SetParametersResult>
  set_parameters_atomically(
    std::vector<rclcpp::parameter::ParameterVariant> parameters,
    std::function<void(
      std::shared_future<rcl_interfaces::SetParametersResult>)> callback = nullptr)
  {
    std::promise<rcl_interfaces::SetParametersResult> promise_result;
    auto future_result = promise_result.get_future().share();

    auto request = std::make_shared<rcl_interfaces::SetParametersAtomically::Request>();

    std::transform(parameters.begin(), parameters.end(), std::back_inserter(
        request->parameters), [](
        rclcpp::parameter::ParameterVariant p) {return p.to_parameter(); });

    set_parameters_atomically_client_->async_send_request(
      request,
      [&promise_result, &future_result, &callback](
        rclcpp::client::Client<rcl_interfaces::SetParametersAtomically>::SharedFuture cb_f) {
          promise_result.set_value(cb_f.get()->result);
          if (callback != nullptr) {
            callback(future_result);
          }
        }
      );

    return future_result;
  }

  std::shared_future<rcl_interfaces::ListParametersResult>
  list_parameters(
    std::vector<std::string> parameter_prefixes,
    uint64_t depth,
    std::function<void(
      std::shared_future<rcl_interfaces::ListParametersResult>)> callback = nullptr)
  {
    std::promise<rcl_interfaces::ListParametersResult> promise_result;
    auto future_result = promise_result.get_future().share();

    auto request = std::make_shared<rcl_interfaces::ListParameters::Request>();
    request->parameter_prefixes = parameter_prefixes;
    request->depth = depth;

    list_parameters_client_->async_send_request(
      request,
      [&promise_result, &future_result, &callback](
        rclcpp::client::Client<rcl_interfaces::ListParameters>::SharedFuture cb_f) {
          promise_result.set_value(cb_f.get()->result);
          if (callback != nullptr) {
            callback(future_result);
          }
        }
      );

    return future_result;
  }

private:
  const rclcpp::node::Node::SharedPtr node_;
  rclcpp::client::Client<rcl_interfaces::GetParameters>::SharedPtr get_parameters_client_;
  rclcpp::client::Client<rcl_interfaces::GetParameterTypes>::SharedPtr get_parameter_types_client_;
  rclcpp::client::Client<rcl_interfaces::SetParameters>::SharedPtr set_parameters_client_;
  rclcpp::client::Client<rcl_interfaces::SetParametersAtomically>::SharedPtr
    set_parameters_atomically_client_;
  rclcpp::client::Client<rcl_interfaces::ListParameters>::SharedPtr list_parameters_client_;
  rclcpp::client::Client<rcl_interfaces::DescribeParameters>::SharedPtr describe_parameters_client_;
  std::string remote_node_name_;
};

class SyncParametersClient
{
public:
  RCLCPP_MAKE_SHARED_DEFINITIONS(SyncParametersClient);

  SyncParametersClient(
    rclcpp::node::Node::SharedPtr & node)
  : node_(node)
  {
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    async_parameters_client_ = std::make_shared<AsyncParametersClient>(node);
  }

  SyncParametersClient(
    rclcpp::executor::Executor::SharedPtr & executor,
    rclcpp::node::Node::SharedPtr & node)
  : executor_(executor), node_(node)
  {
    async_parameters_client_ = std::make_shared<AsyncParametersClient>(node);
  }

  std::vector<rclcpp::parameter::ParameterVariant>
  get_parameters(std::vector<std::string> parameter_names)
  {
    auto f = async_parameters_client_->get_parameters(parameter_names);
    return rclcpp::executors::spin_node_until_future_complete(*executor_, node_, f).get();
  }

  std::vector<rclcpp::parameter::ParameterType>
  get_parameter_types(std::vector<std::string> parameter_names)
  {
    auto f = async_parameters_client_->get_parameter_types(parameter_names);
    return rclcpp::executors::spin_node_until_future_complete(*executor_, node_, f).get();
  }

  std::vector<rcl_interfaces::SetParametersResult>
  set_parameters(std::vector<rclcpp::parameter::ParameterVariant> parameters)
  {
    auto f = async_parameters_client_->set_parameters(parameters);
    return rclcpp::executors::spin_node_until_future_complete(*executor_, node_, f).get();
  }

  rcl_interfaces::SetParametersResult
  set_parameters_atomically(std::vector<rclcpp::parameter::ParameterVariant> parameters)
  {
    auto f = async_parameters_client_->set_parameters_atomically(parameters);
    return rclcpp::executors::spin_node_until_future_complete(*executor_, node_, f).get();
  }

  rcl_interfaces::ListParametersResult
  list_parameters(
    std::vector<std::string> parameter_prefixes,
    uint64_t depth)
  {
    auto f = async_parameters_client_->list_parameters(parameter_prefixes, depth);
    return rclcpp::executors::spin_node_until_future_complete(*executor_, node_, f).get();
  }

private:
  rclcpp::executor::Executor::SharedPtr executor_;
  rclcpp::node::Node::SharedPtr node_;
  AsyncParametersClient::SharedPtr async_parameters_client_;
};

} /* namespace parameter_client */

} /* namespace rclcpp */

#endif /* RCLCPP_RCLCPP_PARAMETER_CLIENT_HPP_ */
