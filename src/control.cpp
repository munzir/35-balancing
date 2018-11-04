/*
 * Copyright (c) 2018, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the Georgia Tech Research Corporation nor
 *       the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GEORGIA TECH RESEARCH CORPORATION ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GEORGIA
 * TECH RESEARCH CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @file control.cpp
 * @author Munzir Zafar
 * @date Oct 31, 2018
 * @brief Implements balancing control functions
 */

#include "control.h"

#include <algorithm>
#include <iostream>

#include <dart/dart.hpp>
#include <Eigen/Eigen>
#include <kore.hpp>
#include <somatic.h>

#include "balancing_config.h"
#include "../../18h-Util/lqr.hpp"
#include "../../18h-Util/adrc.hpp"

/* ******************************************************************************************** */
/// Get the joint values from the encoders and the imu and compute the center of mass as well
void getState(Krang::Hardware* krang_, dart::dynamics::SkeletonPtr robot_,
              Eigen::Matrix<double, 6, 1>& state, double dt, Eigen::Vector3d* com_) {

  // Read motor encoders, imu and ft and update dart skeleton
  krang_->updateSensors(dt);

  // Calculate the COM Using Skeleton
  Eigen::Vector3d com = robot_->getCOM() - robot_->getPositions().segment(3,3);
  if(com_ != NULL) *com_ = com;

  // Update the state (note for amc we are reversing the effect of the motion of the upper body)
  // State are theta, dtheta, x, dx, psi, dpsi
  state(0) = atan2(com(0), com(2)); // - 0.3 * M_PI / 180.0;;
  state(1) = krang_->imuSpeed;
  state(2) = (krang_->amc->pos[0] + krang_->amc->pos[1])/2.0 + krang_->imu;
  state(3) = (krang_->amc->vel[0] + krang_->amc->vel[1])/2.0 + krang_->imuSpeed;
  state(4) = (krang_->amc->pos[1] - krang_->amc->pos[0]) / 2.0;
  state(5) = (krang_->amc->vel[1] - krang_->amc->vel[0]) / 2.0;

  // Making adjustment in com to make it consistent with the hack above for state(0)
  com(0) = com(2) * tan(state(0));
}
/* ******************************************************************************************** */
// // Change robot's beta values (parameters)
dart::dynamics::SkeletonPtr setParameters(dart::dynamics::SkeletonPtr robot_,
                                          Eigen::MatrixXd betaParams, int bodyParams) {
  Eigen::Vector3d bodyMCOM;
  double mi;
  int numBodies = betaParams.cols()/bodyParams;
  for (int i = 0; i < numBodies; i++) {
    mi = betaParams(0, i * bodyParams);
    bodyMCOM(0) = betaParams(0, i * bodyParams + 1);
    bodyMCOM(1) = betaParams(0, i * bodyParams + 2);
    bodyMCOM(2) = betaParams(0, i * bodyParams + 3);

    robot_->getBodyNode(i)->setMass(mi);
    robot_->getBodyNode(i)->setLocalCOM(bodyMCOM/mi);
  }
  return robot_;
}

/* ******************************************************************************************** */
void UpdateReference(const double& forw, const double& spin, const double& dt,
                     Eigen::Matrix<double, 6, 1>& refState) {

      // First, set the balancing angle and velocity to zeroes
      refState(0) = refState(1) = 0.0;

      // Set the distance and heading velocities using the joystick input
      refState(3) = forw;
      refState(5) = spin;

      // Integrate the reference positions with the current reference velocities
      refState(2) += dt * refState(3);
      refState(4) += dt * refState(5);
}
/* ******************************************************************************************** */
void ComputeCurrent(const Eigen::Matrix<double, 6, 1>& K,
                    const Eigen::Matrix<double, 6, 1>& error,
                    double* u_theta, double* u_x, double* u_spin,
                    double* control_input) {

      // Calculate individual components of the control input
      *u_theta = K.topLeftCorner<2,1>().dot(error.topLeftCorner<2,1>());
      *u_x = K(2)*error(2) + K(3)*error(3);
      *u_spin =  -K.bottomLeftCorner<2,1>().dot(error.bottomLeftCorner<2,1>());
      *u_spin = std::max(-30.0, std::min(30.0, *u_spin));

      // Calculate current for the wheels
      control_input[0] = std::max(-49.0, std::min(49.0, *u_theta + *u_x + *u_spin));
      control_input[1] = std::max(-49.0, std::min(49.0, *u_theta + *u_x - *u_spin));
}
/* ******************************************************************************************** */
Eigen::MatrixXd ComputeLqrGains(
    const Krang::Hardware* krang,
    const BalancingConfig& params,
    const Eigen::Matrix<double, 4, 4>& lqrHackRatios) {

  // TODO: Get rid of dynamic allocation
  Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4,4);
  Eigen::MatrixXd B = Eigen::MatrixXd::Zero(4,1);
  Eigen::VectorXd B_thWheel = Eigen::VectorXd::Zero(3);
  Eigen::VectorXd B_thCOM = Eigen::VectorXd::Zero(3);
  Eigen::VectorXd LQR_Gains = Eigen::VectorXd::Zero(4);

  computeLinearizedDynamics(krang->robot, A, B, B_thWheel, B_thCOM);
  lqr(A, B, params.lqrQ, params.lqrR, LQR_Gains);

  const double motor_constant = 12.0 * 0.00706;
  const double gear_ratio = 15;
  LQR_Gains /= (gear_ratio * motor_constant);
  LQR_Gains = lqrHackRatios * LQR_Gains;

  return LQR_Gains;
}
/* ******************************************************************************************** */
void BalancingController(const Krang::Hardware* krang,
                         const Eigen::Matrix<double, 6, 1>& state,
                         const double& dt,
                         const BalancingConfig& params,
                         const Eigen::Matrix<double, 4, 4> lqrHackRatios,
                         const bool joystickControl,
                         const double& js_forw, const double& js_spin,
                         const bool& debug,
                         KRANG_MODE& MODE,
                         Eigen::Matrix<double, 6, 1>& refState,
                         Eigen::Matrix<double, 6, 1>& error,
                         double* control_input) {

  // The timer we use for deciding whether krang has stood up and needs to
  // be switched to BAL_LO mode. This timer is supposed to be zero in all
  // other modes excepts STAND mode. This is to ensure that no matter how we
  // transitioned to STAND mode, this timer is zero in the beginning.
  // In STAND mode this timer starts running if robot is balancing. After it
  // crosses the TimerLimit, mode is switched to balancing.
  static int stood_up_timer = 0;
  const int kStoodUpTimerLimit = 100;
  if(MODE != STAND) stood_up_timer = 0;

  // Controllers for each mode
  Eigen::Matrix<double, 6, 1> K;
  K.setZero();
  double u_x, u_theta, u_spin;
  switch (MODE) {
    case GROUND_LO: {
      //  Update Reference
      double forw, spin;
      forw = params.joystickGainsGroundLo(0) * js_forw;
      spin = params.joystickGainsGroundLo(1) * js_spin;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      error = state - refState;

      // Gains to remain zero if joystickControl (for arms) flag is set
      // otherwise read the fwd and spin gains from parameters
      if(!joystickControl) {
        K.tail(4) = params.pdGainsGroundLo.tail(4);
      }

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

      // State Transition - If the waist has been opened too much switch to
      // GROUND_HI mode
      if((krang->waist->pos[0]-krang->waist->pos[1])/2.0 < 150.0*M_PI/180.0) {
        MODE = GROUND_HI;
      }

      break;
    }
    case GROUND_HI: {

      //  Update Reference
      double forw, spin;
      forw = params.joystickGainsGroundHi(0) * js_forw;
      spin = params.joystickGainsGroundHi(1) * js_spin;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      error = state - refState;

      // Gains to remain zero if joystickControl (for arms) flag is set
      // otherwise read the fwd and spin gains from parameters
      if(!joystickControl) {
        K.tail(4) = params.pdGainsGroundLo.tail(4);
      }

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

      // State Transitions
      // If in ground Hi mode and waist angle decreases below 150.0 goto groundLo mode
      if((krang->waist->pos[0]-krang->waist->pos[1])/2.0 > 150.0*M_PI/180.0) {
        MODE = GROUND_LO;
      }
     break;
    }
    case STAND: {

      //  Update Reference
      double forw, spin;
      forw = 0.0;
      spin = 0.0;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      error = state - refState;

      // Gains - no spinning
      K.head(4) = params.pdGainsStand.head(4);
      if(params.dynamicLQR == true) {
        Eigen::MatrixXd LQR_Gains;
        LQR_Gains = ComputeLqrGains(krang, params, lqrHackRatios);
        K.head(4) = -LQR_Gains;
      }

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

      // State Transition - If stood up go to balancing mode
      if(fabs(state(0)) < 0.034) {
        stood_up_timer++;
      }
      else {
        stood_up_timer = 0;
      }
      if(stood_up_timer > kStoodUpTimerLimit) {
        MODE = BAL_LO;
      }

     break;
    }
    case BAL_LO: {

      //  Update Reference
      double forw, spin;
      forw = params.joystickGainsBalLo(0) * js_forw;
      spin = params.joystickGainsBalLo(1) * js_spin;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      error = state - refState;

      // Gains
      K = params.pdGainsBalLo;
      if(params.dynamicLQR == true) {
        Eigen::MatrixXd LQR_Gains;
        LQR_Gains = ComputeLqrGains(krang, params, lqrHackRatios);
        K.head(4) = -LQR_Gains;
      }

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

     break;
    }
    case BAL_HI: {

      //  Update Reference
      double forw, spin;
      forw = params.joystickGainsBalHi(0) * js_forw;
      spin = params.joystickGainsBalHi(1) * js_spin;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      error = state - refState;

      // Gains
      K = params.pdGainsBalHi;
      if(params.dynamicLQR == true) {
        Eigen::MatrixXd LQR_Gains;
        LQR_Gains = ComputeLqrGains(krang, params, lqrHackRatios);
        K.head(4) = -LQR_Gains;
      }

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

     break;
    }
    case SIT: {

      //  Update Reference
      double forw, spin;
      forw = 0.0;
      spin = 0.0;
      UpdateReference(forw, spin, dt, refState);

      // Calculate state Error
      const double kImuSitAngle = ((-103.0 / 180.0) * M_PI);
      error = state - refState;
      error(0) = krang->imu - kImuSitAngle;

      // Gains - turn off fwd and spin control i.e. only control theta
      K.head(2) = params.pdGainsSit.head(2);

      // Compute the current
      ComputeCurrent(K, error, &u_theta, &u_x, &u_spin, &control_input[0]);

      // State Transitions - If sat down switch to Ground Lo Mode
      if(krang->imu < kImuSitAngle) {
        std::cout << "imu (" << krang->imu << ") < limit (" << kImuSitAngle << "):";
        std::cout << "changing to Ground Lo Mode" << std::endl;
        MODE = GROUND_LO;
      }

     break;
    }
  }
  if(debug) {
    std::cout << "refState: " << refState.transpose() << std::endl;
    std::cout << "error: " << error.transpose();
    std::cout << ", imu: " << krang->imu / M_PI * 180.0 << std::endl;
    std::cout << "K: " << K.transpose() << std::endl;
  }
}
