#include "drake/geometry/optimization/point.h"

#include <memory>

#include <fmt/format.h>

#include "drake/common/is_approx_equal_abstol.h"

namespace drake {
namespace geometry {
namespace optimization {

using Eigen::MatrixXd;
using Eigen::VectorXd;
using math::RigidTransformd;
using solvers::Binding;
using solvers::Constraint;
using solvers::MathematicalProgram;
using solvers::VectorXDecisionVariable;
using std::sqrt;
using symbolic::Variable;

Point::Point(const Eigen::Ref<const Eigen::VectorXd>& x)
    : ConvexSet(&ConvexSetCloner<Point>, x.size()), x_{x} {}

Point::Point(const QueryObject<double>& query_object, GeometryId geometry_id,
             std::optional<FrameId> expressed_in,
             double maximum_allowable_radius)
    : ConvexSet(&ConvexSetCloner<Point>, 3) {
  double radius = -1.0;
  query_object.inspector().GetShape(geometry_id).Reify(this, &radius);
  if (radius > maximum_allowable_radius) {
    throw std::runtime_error(
        fmt::format("GeometryID {} has a radius {} is larger than the "
                    "specified maximum_allowable_radius: {}.",
                    geometry_id, radius, maximum_allowable_radius));
  }

  const RigidTransformd X_WE = expressed_in
                                   ? query_object.GetPoseInWorld(*expressed_in)
                                   : RigidTransformd::Identity();
  const RigidTransformd& X_WG = query_object.GetPoseInWorld(geometry_id);
  const RigidTransformd X_EG = X_WE.InvertAndCompose(X_WG);
  x_ = X_EG.translation();
}

Point::~Point() = default;

bool Point::DoPointInSet(const Eigen::Ref<const Eigen::VectorXd>& x,
                         double tol) const {
  return is_approx_equal_abstol(x, x_, tol);
}

void Point::DoAddPointInSetConstraint(
    MathematicalProgram* prog,
    const Eigen::Ref<const VectorXDecisionVariable>& x) const {
  prog->AddBoundingBoxConstraint(x_, x_, x);
}

std::vector<Binding<Constraint>>
Point::DoAddPointInNonnegativeScalingConstraints(
    MathematicalProgram* prog,
    const Eigen::Ref<const VectorXDecisionVariable>& x,
    const symbolic::Variable& t) const {
  std::vector<Binding<Constraint>> constraints;
  // x == t*x_.
  const int n = ambient_dimension();
  MatrixXd Aeq(n, n + 1);
  Aeq.leftCols(n) = MatrixXd::Identity(n, n);
  Aeq.col(n) = -x_;
  constraints.emplace_back(prog->AddLinearEqualityConstraint(
      Aeq, VectorXd::Zero(n), {x, Vector1<Variable>(t)}));
  return constraints;
}

std::pair<std::unique_ptr<Shape>, math::RigidTransformd>
Point::DoToShapeWithPose() const {
  return std::make_pair(std::make_unique<Sphere>(0.0),
                        math::RigidTransformd(x_));
}

void Point::ImplementGeometry(const Sphere& sphere, void* data) {
  double* radius = static_cast<double*>(data);
  *radius = sphere.radius();
}

}  // namespace optimization
}  // namespace geometry
}  // namespace drake