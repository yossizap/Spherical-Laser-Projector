void Matrix::zoom(double factor) {
  Matrix other(
    Vector(factor, 0, 0),
    Vector(0, factor, 0),
    Vector(0, 0, factor));
  multiply(other);
}

void Matrix::rotate(double theta) {
  Matrix other(
    Vector(cos(theta), sin(theta), 0),
    Vector(-sin(theta), cos(theta), 0),
    Vector(0, 0, 1));
  multiply(other);
}

void Matrix::translate(double dx, double dy) {
  Matrix other(
    Vector(1, 0, dx),
    Vector(0, 1, dy),
    Vector(0, 0, 1));
  multiply(other);
}

void Matrix::multiply(const Matrix &that) {
  Matrix result(
    Vector(
      (that.x_.x_ * x_.x_) + (that.x_.y_ * y_.x_) + (that.x_.w_ * w_.x_),
      (that.x_.x_ * x_.y_) + (that.x_.y_ * y_.y_) + (that.x_.w_ * w_.y_),
      (that.x_.x_ * x_.w_) + (that.x_.y_ * y_.w_) + (that.x_.w_ * w_.w_)),
    Vector(
      (that.y_.x_ * x_.x_) + (that.y_.y_ * y_.x_) + (that.y_.w_ * w_.x_),
      (that.y_.x_ * x_.y_) + (that.y_.y_ * y_.y_) + (that.y_.w_ * w_.y_),
      (that.y_.x_ * x_.w_) + (that.y_.y_ * y_.w_) + (that.y_.w_ * w_.w_)),
    Vector(
      (that.w_.x_ * x_.x_) + (that.w_.y_ * y_.x_) + (that.w_.w_ * w_.x_),
      (that.w_.x_ * x_.y_) + (that.w_.y_ * y_.y_) + (that.w_.w_ * w_.y_),
      (that.w_.x_ * x_.w_) + (that.w_.y_ * y_.w_) + (that.w_.w_ * w_.w_)));
  *this = result;
}

void Matrix::reset() {
  x_ = Vector(1, 0, 0);
  y_ = Vector(0, 1, 0);
  w_ = Vector(0, 0, 1);
}




