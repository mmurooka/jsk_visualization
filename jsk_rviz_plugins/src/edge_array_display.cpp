/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Ryohei Ueda and JSK Lab
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
 *     disclaimer in the documentation and/o2r other materials provided
 *     with the distribution.
 *   * Neither the name of the JSK Lab nor the names of its
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
 *********************************************************************/

#include "edge_array_display.h"
#include <jsk_topic_tools/color_utils.h>
namespace jsk_rviz_plugins
{
  EdgeArrayDisplay::EdgeArrayDisplay()
  {
    coloring_property_ = new rviz::EnumProperty(
      "coloring", "Auto",
      "coloring method",
      this, SLOT(updateColoring()));
    coloring_property_->addOption("Auto", 0);
    coloring_property_->addOption("Flat color", 1);

    color_property_ = new rviz::ColorProperty(
      "color", QColor(25, 255, 0),
      "color to draw the edges",
      this, SLOT(updateColor()));
    alpha_property_ = new rviz::FloatProperty(
      "alpha", 0.8,
      "alpha value to draw the edges",
      this, SLOT(updateAlpha()));
    line_width_property_ = new rviz::FloatProperty(
      "line width", 0.005,
      "line width of the edges",
      this, SLOT(updateLineWidth()));
  }

  EdgeArrayDisplay::~EdgeArrayDisplay()
  {
    delete color_property_;
    delete alpha_property_;
    delete coloring_property_;
  }

  QColor EdgeArrayDisplay::getColor(size_t index)
  {
    if (coloring_method_ == "auto") {
      std_msgs::ColorRGBA ros_color = jsk_topic_tools::colorCategory20(index);
      return QColor(ros_color.r * 255.0,
                    ros_color.g * 255.0,
                    ros_color.b * 255.0,
                    ros_color.a * 255.0);
    }
    else if (coloring_method_ == "flat") {
      return color_;
    }
  }

  void EdgeArrayDisplay::onInitialize()
  {
    MFDClass::onInitialize();
    scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();

    updateColor();
    updateAlpha();
    updateColoring();
    updateLineWidth();
  }

  void EdgeArrayDisplay::updateLineWidth()
  {
    line_width_ = line_width_property_->getFloat();
    if (latest_msg_) {
      processMessage(latest_msg_);
    }
  }

  void EdgeArrayDisplay::updateColor()
  {
    color_ = color_property_->getColor();
    if (latest_msg_) {
      processMessage(latest_msg_);
    }
  }

  void EdgeArrayDisplay::updateAlpha()
  {
    alpha_ = alpha_property_->getFloat();
    if (latest_msg_) {
      processMessage(latest_msg_);
    }
  }

  void EdgeArrayDisplay::updateColoring()
  {
    if (coloring_property_->getOptionInt() == 0) {
      coloring_method_ = "auto";
      color_property_->hide();
    }
    else if (coloring_property_->getOptionInt() == 1) {
      coloring_method_ = "flat";
      color_property_->show();
    }

    if (latest_msg_) {
      processMessage(latest_msg_);
    }
  }

  void EdgeArrayDisplay::reset()
  {
    MFDClass::reset();
    edges_.clear();
    latest_msg_.reset();
  }

  void EdgeArrayDisplay::allocateLines(int num)
  {
    if (num > edges_.size()) {
      for (size_t i = edges_.size(); i < num; i++) {
        LinePtr line(new rviz::Line(
                                context_->getSceneManager(), scene_node_));
        edges_.push_back(line);
      }
    }
    else if (num < edges_.size())
      {
        edges_.resize(num);       // ok??
      }
  }

  void EdgeArrayDisplay::showEdges(
    const jsk_recognition_msgs::ModelCoefficientsArray::ConstPtr& msg)
  {
    allocateLines(msg->coefficients.size());
    for (size_t i = 0; i < msg->coefficients.size(); i++) {
      pcl_msgs::ModelCoefficients edge_coeff = msg->coefficients[i];

      LinePtr edge = edges_[i];

      Ogre::Vector3 center_local(edge_coeff.values[0], edge_coeff.values[1], edge_coeff.values[2]);
      Ogre::Vector3 dir_local(edge_coeff.values[3], edge_coeff.values[4], edge_coeff.values[5]);
      Ogre::Vector3 start_point_local(center_local + dir_local);
      Ogre::Vector3 end_point_local(center_local - dir_local);

      geometry_msgs::Pose start_pose_local;
      geometry_msgs::Pose end_pose_local;
      start_pose_local.position.x = start_point_local[0];
      start_pose_local.position.y = start_point_local[1];
      start_pose_local.position.z = start_point_local[2];
      start_pose_local.orientation.w = 1.0;
      end_pose_local.position.x = end_point_local[0];
      end_pose_local.position.y = end_point_local[1];
      end_pose_local.position.z = end_point_local[2];
      end_pose_local.orientation.w = 1.0;

      Ogre::Vector3 start_point;
      Ogre::Vector3 end_point;
      Ogre::Quaternion quaternion; // not used to visualize
      bool transform_ret;
      transform_ret =
        context_->getFrameManager()->transform(edge_coeff.header, start_pose_local, start_point, quaternion)
        && context_->getFrameManager()->transform(edge_coeff.header, end_pose_local, end_point, quaternion);
        if(!transform_ret) {
          ROS_ERROR( "Error transforming pose"
                     "'%s' from frame '%s' to frame '%s'",
                     qPrintable( getName() ), edge_coeff.header.frame_id.c_str(),
                     qPrintable( fixed_frame_ ));
          return;                 // return?
        }
      edge->setPoints(start_point, end_point);
      QColor color = getColor(i);
      edge->setColor(color.red() / 255.0,
                     color.green() / 255.0,
                     color.blue() / 255.0,
                     alpha_);
    }
  }

  void EdgeArrayDisplay::processMessage(
    const jsk_recognition_msgs::ModelCoefficientsArray::ConstPtr& msg)
  {
    // Store latest message
    latest_msg_ = msg;

    showEdges(msg);
  }
}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(
  jsk_rviz_plugins::EdgeArrayDisplay, rviz::Display)
