// error helper (depends V8 environment)

function createStackTrace(inst, ctor) {
	if (!(inst instanceof Error)) return;
	if (!(typeof ctor === 'function')) return;
	if (!Error.captureStackTrace) return;

	Error.captureStackTrace(inst, ctor);
}

module.exports.createStackTrace = createStackTrace;
