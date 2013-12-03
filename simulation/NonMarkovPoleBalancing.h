/*
 * NonMarkovPoleBalancing.h
 *
 *  Created on: Oct 19, 2013
 *      Author: sam
 */

#ifndef NONMARKOVPOLEBALANCING_H_
#define NONMARKOVPOLEBALANCING_H_

#include "RL.h"

using namespace RLLib;
/**
 * Incremental Evolution of Complex General Behavior
 * Faustino Gomez and Risto Miikkulainen, 1996
 *
 */

template<class T>
class NonMarkovPoleBalancing: public RLProblem<T>
{
    typedef RLProblem<T> Base;
  protected:
    int nbPoles;
    bool random;
    float stepTime; // s
    float x; // m
    float xDot; // ms^{-1}
    float g; // ms^{-2}
    float M; // Kg
    float muc; // coefficient of friction of cart on track
    float threeFourth;
    float fifteenRadian;
    float twelveRadian;

    Range<float>* xRange;
    Range<float>* thetaRange;
    Range<float>* actionRange;

  public:
    Vector<float>* theta;
    Vector<float>* thetaDot;
    Vector<float>* length; // half length of i^{th} pole
    Vector<float>* effectiveForce; // effective force
    Vector<float>* mass; // mass of i^{th} pole
    Vector<float>* effectiveMass; // effective mass
    Vector<float>* mup; // coefficient of friction of i^{th} pole's hinge

  public:
    NonMarkovPoleBalancing(const int& nbPoles = 1, const bool& random = false) :
        RLProblem<T>((1 + nbPoles) * 2, 3, 1), nbPoles(nbPoles), random(random), stepTime(0.02), x(
            0), xDot(0), g(-9.81), M(1.0), muc(0), threeFourth(3.0f / 4.0f), fifteenRadian(
            15.0f / 180.0f * M_PI), twelveRadian(12.0f / 180.0f * M_PI), xRange(
            new Range<float>(-2.4f, 2.4f)), actionRange(new Range<float>(-10.0f, 10.0f)), theta(
            new PVector<float>(nbPoles)), thetaDot(new PVector<float>(nbPoles)), length(
            new PVector<float>(nbPoles)), effectiveForce(new PVector<float>(nbPoles)), mass(
            new PVector<float>(nbPoles)), effectiveMass(new PVector<float>(nbPoles)), mup(
            new PVector<float>(nbPoles))
    {
      assert(nbPoles <= 2);
      if (nbPoles == 2)
      {
        thetaRange = new Range<float>(-fifteenRadian, fifteenRadian);
        length->setEntry(0, 0.5);
        length->setEntry(1, 0.05);
        mass->setEntry(0, 0.1);
        mass->setEntry(1, 0.01);
        muc = 0.0005f;
        mup->setEntry(0, 0.000002f);
        mup->setEntry(1, 0.000002f);
      }
      else
      {
        thetaRange = new Range<float>(-twelveRadian, twelveRadian);
        length->setEntry(0, 0.5); // Kg
        mass->setEntry(0, 0.1); // Kg
        muc = 0.0;
      }

      Base::discreteActions->push_back(0, actionRange->min());
      Base::discreteActions->push_back(1, 0.0);
      Base::discreteActions->push_back(2, actionRange->max());

      // subject to change
      Base::continuousActions->push_back(0, 0.0);
    }

    ~NonMarkovPoleBalancing()
    {
      delete xRange;
      delete thetaRange;
      delete actionRange;
      delete theta;
      delete thetaDot;
      delete length;
      delete effectiveForce;
      delete mass;
      delete effectiveMass;
      delete mup;
    }

  private:
    void adjustTheta()
    {
      for (int i = 0; i < nbPoles; i++)
      {
        if (theta->getEntry(i) >= M_PI)
          theta->setEntry(i, theta->getEntry(i) - 2.0 * M_PI);
        if (theta->getEntry(i) < -M_PI)
          theta->setEntry(i, theta->getEntry(i) + 2.0 * M_PI);
      }
    }

  public:
    void updateRTStep()
    {
      Base::output->updateRTStep(r(), z(), endOfEpisode());
      DenseVector<T>& vars = *Base::output->o_tp1;

      Base::observations->at(0) = xRange->bound(x);
      Base::observations->at(1) = xDot;
      vars[0] = Base::observations->at(0); //<<FixMe: only for testing
      vars[1] = Base::observations->at(1);

      for (int i = 0; i < nbPoles; i += 2)
      {
        Base::observations->at(i + 2) = theta->getEntry(i);
        Base::observations->at(i + 3) = thetaDot->getEntry(i);
        vars[i + 2] = Base::observations->at(i + 2); //<<FixMe: only for testing
        vars[i + 3] = Base::observations->at(i + 3);
      }

    }

    void initialize()
    {
      if (random)
      {
        Range<float> xs1(-2, 2);
        Range<float> thetas1(-0.6, 0.6);
        Range<float> xs2(-0.2, 0.2);
        Range<float> thetas2(-0.2, 0.2);

        //<<FixMe: S2
        x = xs2.chooseRandom();
        for (int i = 0; i < nbPoles; i++)
          theta->setEntry(i, thetas2.chooseRandom());
      }
      else
      {
        x = 0.0;
        for (int i = 0; i < nbPoles; i++)
          theta->setEntry(i, 0.0f);
      }

      xDot = 0;
      for (int i = 0; i < nbPoles; i++)
        thetaDot->setEntry(i, 0.0f);

      adjustTheta();
      updateRTStep();
    }

    void step(const Action<T>* a)
    {
      float totalEffectiveForce = 0;
      float totalEffectiveMass = 0;
      for (int i = 0; i < nbPoles; i++)
      {
        double effForce = mass->getEntry(i) * length->getEntry(i) * pow(thetaDot->getEntry(i), 2)
            * sin(theta->getEntry(i));
        effForce += threeFourth * mass->getEntry(i) * cos(theta->getEntry(i))
            * ((mup->getEntry(i) * thetaDot->getEntry(i))
                / (mass->getEntry(i) * length->getEntry(i)) + g * sin(theta->getEntry(i)));
        effectiveForce->setEntry(i, effForce);
        effectiveMass->setEntry(i,
            mass->getEntry(i) * (1.0f - threeFourth * pow(cos(theta->getEntry(i)), 2)));
        totalEffectiveForce += effectiveForce->getEntry(i);
        totalEffectiveMass += effectiveMass->getEntry(i);
      }

      float torque = actionRange->bound(a->at(0));
      float xAcc = (torque - muc * Signum::valueOf(xDot) + totalEffectiveForce)
          / (M + totalEffectiveMass);

      // Update the four state variables, using Euler's method.
      x += xDot * stepTime;
      xDot += xAcc * stepTime;

      for (int i = 0; i < nbPoles; i++)
      {
        float thetaDotDot = -threeFourth
            * (xAcc * cos(theta->getEntry(i)) + g * sin(theta->getEntry(i))
                + (mup->getEntry(i) * thetaDot->getEntry(i))
                    / (mass->getEntry(i) * length->getEntry(i))) / length->getEntry(i);

        // Update the four state variables, using Euler's method.
        theta->setEntry(i, theta->getEntry(i) + thetaDot->getEntry(i) * stepTime);
        thetaDot->setEntry(i, thetaDot->getEntry(i) + thetaDotDot * stepTime);
      }

      adjustTheta();
      updateRTStep();
    }

    bool endOfEpisode() const
    {
      bool value = true;
      for (int i = 0; i < nbPoles; i++)
        value *= thetaRange->in(theta->getEntry(i));
      value = value && xRange->in(x);
      return !value;
    }

    float r() const
    {
      float value = 0;
      for (int i = 0; i < nbPoles; i++)
        value += cos(theta->getEntry(i));
      return value;
    }

    float z() const
    {
      return 0.0;
    }

};

#endif /* NONMARKOVPOLEBALANCING_H_ */
