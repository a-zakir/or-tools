// Copyright 2010-2022 Google LLC
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

#include "ortools/glop/dual_edge_norms.h"

#include <cstdlib>

#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

DualEdgeNorms::DualEdgeNorms(const BasisFactorization& basis_factorization)
    : basis_factorization_(basis_factorization),
      recompute_edge_squared_norms_(true) {}

bool DualEdgeNorms::NeedsBasisRefactorization() const {
  return recompute_edge_squared_norms_;
}

void DualEdgeNorms::Clear() { recompute_edge_squared_norms_ = true; }

void DualEdgeNorms::ResizeOnNewRows(RowIndex new_size) {
  edge_squared_norms_.resize(new_size, 1.0);
}

DenseColumn::ConstView DualEdgeNorms::GetEdgeSquaredNorms() {
  if (recompute_edge_squared_norms_) ComputeEdgeSquaredNorms();
  return edge_squared_norms_.const_view();
}

void DualEdgeNorms::UpdateDataOnBasisPermutation(
    const ColumnPermutation& col_perm) {
  if (recompute_edge_squared_norms_) return;
  ApplyColumnPermutationToRowIndexedVector(col_perm, &edge_squared_norms_,
                                           &tmp_edge_squared_norms_);
}

bool DualEdgeNorms::TestPrecision(RowIndex leaving_row,
                                  const ScatteredRow& unit_row_left_inverse) {
  // This should only be true if we do not use steepest edge.
  if (recompute_edge_squared_norms_) return true;

  // ||unit_row_left_inverse||^2 is the same as
  // edge_squared_norms_[leaving_row], but with a better precision. If the
  // difference between the two is too large, we trigger a full recomputation.
  const Fractional leaving_squared_norm =
      SquaredNorm(TransposedView(unit_row_left_inverse));
  const Fractional old_squared_norm = edge_squared_norms_[leaving_row];
  const Fractional estimated_edge_norms_accuracy =
      (sqrt(leaving_squared_norm) - sqrt(old_squared_norm)) /
      sqrt(leaving_squared_norm);
  stats_.edge_norms_accuracy.Add(estimated_edge_norms_accuracy);

  if (std::abs(estimated_edge_norms_accuracy) >
      parameters_.recompute_edges_norm_threshold()) {
    VLOG(1) << "Recomputing edge norms: " << sqrt(leaving_squared_norm)
            << " vs " << sqrt(old_squared_norm);
    recompute_edge_squared_norms_ = true;
  }

  // We just do not want to pivot on a position with an under-estimated norm.
  edge_squared_norms_[leaving_row] = leaving_squared_norm;
  const bool result = old_squared_norm > 0.25 * leaving_squared_norm;
  if (!result) {
    VLOG(1) << "Recomputing leaving row. Norm was " << sqrt(old_squared_norm)
            << " vs precise version " << sqrt(leaving_squared_norm);
  }
  return result;
}

void DualEdgeNorms::UpdateBeforeBasisPivot(
    ColIndex entering_col, RowIndex leaving_row,
    const ScatteredColumn& direction,
    const ScatteredRow& unit_row_left_inverse) {
  // No need to update if we will recompute it from scratch later.
  if (recompute_edge_squared_norms_) return;
  const DenseColumn& tau = ComputeTau(TransposedView(unit_row_left_inverse));
  SCOPED_TIME_STAT(&stats_);

  const Fractional pivot = direction[leaving_row];
  const Fractional new_leaving_squared_norm =
      edge_squared_norms_[leaving_row] / Square(pivot);

  // Update the norm.
  int stat_lower_bounded_norms = 0;
  auto output = edge_squared_norms_.view();
  for (const auto e : direction) {
    // Note that the update formula used is important to maximize the precision.
    // See Koberstein's PhD section 8.2.2.1.
    output[e.row()] +=
        e.coefficient() * (e.coefficient() * new_leaving_squared_norm -
                           2.0 / pivot * tau[e.row()]);

    // Avoid 0.0 norms (The 1e-4 is the value used by Koberstein).
    // TODO(user): use a more precise lower bound depending on the column norm?
    // We can do that with Cauchy-Swartz inequality:
    //   (edge . leaving_column)^2 = 1.0 < ||edge||^2 * ||leaving_column||^2
    const Fractional kLowerBound = 1e-4;
    if (output[e.row()] < kLowerBound) {
      if (e.row() == leaving_row) continue;
      output[e.row()] = kLowerBound;
      ++stat_lower_bounded_norms;
    }
  }
  output[leaving_row] = new_leaving_squared_norm;
  IF_STATS_ENABLED(stats_.lower_bounded_norms.Add(stat_lower_bounded_norms));
}

void DualEdgeNorms::ComputeEdgeSquaredNorms() {
  SCOPED_TIME_STAT(&stats_);

  // Since we will do a lot of inversions, it is better to be as efficient and
  // precise as possible by having a refactorized basis.
  DCHECK(basis_factorization_.IsRefactorized());
  const RowIndex num_rows = basis_factorization_.GetNumberOfRows();
  edge_squared_norms_.resize(num_rows, 0.0);
  for (RowIndex row(0); row < num_rows; ++row) {
    edge_squared_norms_[row] = basis_factorization_.DualEdgeSquaredNorm(row);
  }
  recompute_edge_squared_norms_ = false;
}

const DenseColumn& DualEdgeNorms::ComputeTau(
    const ScatteredColumn& unit_row_left_inverse) {
  SCOPED_TIME_STAT(&stats_);
  const DenseColumn& result =
      basis_factorization_.RightSolveForTau(unit_row_left_inverse);
  IF_STATS_ENABLED(stats_.tau_density.Add(Density(Transpose(result))));
  return result;
}

}  // namespace glop
}  // namespace operations_research
