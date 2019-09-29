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

#ifndef RCLCPP__LOANED_MESSAGE_HPP_
#define RCLCPP__LOANED_MESSAGE_HPP_

#include <memory>
#include <utility>

#include "rclcpp/logging.hpp"
#include "rclcpp/publisher_base.hpp"

#include "rcl/allocator.h"
#include "rcl/publisher.h"

namespace rclcpp
{

template<typename MessageT, typename AllocatorT = std::allocator<void>>
class LoanedMessage
{
  using MessageAllocatorTraits = allocator::AllocRebind<MessageT, AllocatorT>;
  using MessageAllocator = typename MessageAllocatorTraits::allocator_type;

protected:
  const rclcpp::PublisherBase * pub_;

  std::unique_ptr<MessageT> message_;

  const std::shared_ptr<MessageAllocator> message_allocator_;

  /// Deleted copy constructor to preserve memory integrity
  LoanedMessage(const LoanedMessage<MessageT> & other) = delete;

public:
  /// Constructor of the LoanedMessage class
  /**
   * The constructor of this class allocates memory for a given message type
   * and associates this with a given publisher.
   *
   * \Note: Given the publisher instance, a case differentiation is being performaned
   * which decides whether the underlying middleware is able to allocate the appropriate
   * memory for this message type or not.
   * In the case that the middleware can not loan messages, the passed in allocator instance
   * is being used to allocate the message within the scope of this class.
   * Otherwise, the allocator is being ignored and the allocation is solely performaned
   * in the underlying middleware with its appropriate allocation strategy.
   * The need for this arises as the user code can be written explicitly targeting a middleware
   * capable of loaning messages.
   * However, this user code is ought to be usable even when dynamically linked against
   * a middleware which doesn't support message loaning in which case the allocator will be used.
   *
   * \param pub rclcpp::Publisher instance to which the memory belongs
   * \param allocator Allocator instance in case middleware can not allocate messages
   */
  LoanedMessage(
    const rclcpp::PublisherBase * pub,
    const std::shared_ptr<std::allocator<MessageT>> allocator)
  : pub_(pub),
    message_(nullptr),
    message_allocator_(allocator)
  {
    if (!pub) {
      throw std::runtime_error("publisher pointer is null");
    }

    void * message_memory = nullptr;
    if (pub_->can_loan_messages()) {
      message_memory =
        rcl_allocate_loaned_message(pub_->get_publisher_handle(), nullptr, sizeof(MessageT));
    } else {
      RCLCPP_WARN(
        rclcpp::get_logger("rclcpp"),
        "Currently used middleware can't loan messages. Local allocator will be used.");
      message_memory = message_allocator_->allocate(1);
    }
    if (!message_memory) {
      throw std::runtime_error("unable to allocate memory for loaned message");
    }
    message_.reset(new (message_memory) MessageT());
  }

  /// Move semantic for RVO
  LoanedMessage(LoanedMessage<MessageT> && other)
  : pub_(std::move(other.pub_)),
    message_(std::move(other.message_)),
    message_allocator_(std::move(other.message_allocator_))
  {}

  /// Destructor of the LoanedMessage class
  /**
   * The destructor has the explicit task to return the allocated memory for its message
   * instance.
   * If the message was previously allocated via the middleware, the message is getting
   * returned to the middleware to cleanly destroy the allocation.
   * In the case that the local allocator instance was used, the same instance is then
   * being used to destroy the allocated memory.
   *
   * The contract here is that the memory for this message is valid as long as this instance
   * of the LoanedMessage class is alive.
   */
  virtual ~LoanedMessage()
  {
    auto error_logger = rclcpp::get_logger("LoanedMessage");
    if (!pub_) {
      RCLCPP_ERROR(error_logger, "Can't deallocate message memory. Publisher instance is NULL");
      return;
    }

    // release allocated memory from unique_ptr
    MessageT * message_memory = message_.release();

    if (pub_->can_loan_messages()) {
      // return allocated memory to the middleware
      auto ret =
        rcl_deallocate_loaned_message(pub_->get_publisher_handle(), message_memory);
      if (ret != RCL_RET_OK) {
        RCLCPP_ERROR(
          error_logger, "rcl_deallocate_loaned_message failed: %s", rcl_get_error_string().str);
        rcl_reset_error();
        return;
      }
    } else {
      message_allocator_->deallocate(message_memory, 1);
    }
    message_memory = nullptr;
    message_ = nullptr;
  }

  /// Validate if the message was correctly allocated
  /**
   * The allocated memory might not be always consistent and valid.
   * Reasons why this could fail is that an allocation step was failing,
   * e.g. just like malloc could fail or a maximum amount of previously allocated
   * messages is exceeded in which case the loaned messages have to be returned
   * to the middleware prior to be able to allocate a new one.
   */
  bool is_valid() const
  {
    return message_ != nullptr;
  }

  /// Access the ROS message instance
  /**
   * A call to `get()` will return a mutable reference to the underlying ROS message instance.
   * This allows a user to modify the content of the message prior to publishing it.
   *
   * \Note: If this reference is copied, the memory for this copy is no longer managed
   * by the LoanedMessage instance and has to be cleanup individually.
   */
  MessageT & get() const
  {
    return *message_;
  }
};

}  // namespace rclcpp

#endif  // RCLCPP__LOANED_MESSAGE_HPP_
