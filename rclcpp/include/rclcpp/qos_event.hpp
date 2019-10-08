// Copyright 2019 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP__QOS_EVENT_HPP_
#define RCLCPP__QOS_EVENT_HPP_

#include <functional>

#include "rcl/error_handling.h"

#include "rcutils/logging_macros.h"

#include "rclcpp/exceptions.hpp"
#include "rclcpp/function_traits.hpp"
#include "rclcpp/waitable.hpp"

namespace rclcpp
{

using QOSDeadlineRequestedInfo = rmw_requested_deadline_missed_status_t;
using QOSDeadlineOfferedInfo = rmw_offered_deadline_missed_status_t;
using QOSLivelinessChangedInfo = rmw_liveliness_changed_status_t;
using QOSLivelinessLostInfo = rmw_liveliness_lost_status_t;

using QOSDeadlineRequestedCallbackType = std::function<void (QOSDeadlineRequestedInfo &)>;
using QOSDeadlineOfferedCallbackType = std::function<void (QOSDeadlineOfferedInfo &)>;
using QOSLivelinessChangedCallbackType = std::function<void (QOSLivelinessChangedInfo &)>;
using QOSLivelinessLostCallbackType = std::function<void (QOSLivelinessLostInfo &)>;

/// Contains callbacks for various types of events a Publisher can receive from the middleware.
struct PublisherEventCallbacks
{
  QOSDeadlineOfferedCallbackType deadline_callback;
  QOSLivelinessLostCallbackType liveliness_callback;
};

/// Contains callbacks for non-message events that a Subscription can receive from the middleware.
struct SubscriptionEventCallbacks
{
  QOSDeadlineRequestedCallbackType deadline_callback;
  QOSLivelinessChangedCallbackType liveliness_callback;
};

class QOSEventHandlerBase : public Waitable
{
public:
  RCLCPP_PUBLIC
  virtual ~QOSEventHandlerBase();

  /// Get the number of ready events
  RCLCPP_PUBLIC
  size_t
  get_number_of_ready_events() override;

  /// Check if the Waitable is ready.
  RCLCPP_PUBLIC
  bool
  is_ready(rcl_wait_set_t * wait_set) override;

protected:
  rcl_event_t event_handle_;
  size_t wait_set_event_index_;
};

template<typename EventCallbackT>
class QOSEventHandler : public QOSEventHandlerBase
{
public:
  template<typename InitFuncT, typename ParentHandleT, typename EventTypeEnum>
  QOSEventHandler(
    const EventCallbackT & callback,
    InitFuncT init_func,
    ParentHandleT parent_handle,
    EventTypeEnum event_type)
  : event_callback_(callback)
  {
    event_handle_ = rcl_get_zero_initialized_event();
    rcl_ret_t ret = init_func(&event_handle_, parent_handle, event_type);
    if (ret != RCL_RET_OK) {
      rclcpp::exceptions::throw_from_rcl_error(ret, "could not create event");
    }
  }

  /// Execute any entities of the Waitable that are ready.
  void
  execute() override
  {
    EventCallbackInfoT callback_info;

    rcl_ret_t ret = rcl_take_event(&event_handle_, &callback_info);
    if (ret != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        "rclcpp",
        "Couldn't take event info: %s", rcl_get_error_string().str);
      return;
    }

    event_callback_(callback_info);
  }

private:
  using EventCallbackInfoT = typename std::remove_reference<typename
      rclcpp::function_traits::function_traits<EventCallbackT>::template argument_type<0>>::type;

  EventCallbackT event_callback_;
};

}  // namespace rclcpp

#endif  // RCLCPP__QOS_EVENT_HPP_
