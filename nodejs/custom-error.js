const {
	createStackTrace,
} = require('./deps/V8/error-helper');

// custom error
function CodingError(message = '') {
	if (!(this instanceof CodingError)) new CodingError(message);
	TypeError.call(this, message);
	this.name = 'CodingError';
	this.message = message;
	createStackTrace(this, CodingError);
}
Object.setPrototypeOf(CodingError.prototype, TypeError.prototype);

module.exports.CodingError = CodingError;
