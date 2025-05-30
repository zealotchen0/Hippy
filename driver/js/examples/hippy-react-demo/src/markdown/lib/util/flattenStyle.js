/**
 * Flattens nested style objects into a single style object
 * @param {Object} style - The style object to flatten
 * @returns {Object} - The flattened style object
 */
const flattenStyle = (style) => {
  if (!style) {
    return {};
  }

  // If style is an array, flatten each item and merge them
  if (Array.isArray(style)) {
    return style.reduce((acc, curr) => ({
      ...acc,
      ...flattenStyle(curr),
    }), {});
  }

  // If style is an object, process it
  if (typeof style === 'object') {
    const flattened = {};

    // Iterate through all properties
    Object.keys(style).forEach((key) => {
      const value = style[key];

      // If the value is an object or array, recursively flatten it
      if (value && typeof value === 'object') {
        Object.assign(flattened, flattenStyle(value));
      } else {
        // Otherwise, just copy the value
        flattened[key] = value;
      }
    });

    return flattened;
  }

  return {};
};

export default flattenStyle;
