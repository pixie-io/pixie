/*eslint-disable block-scoped-var, id-length, no-control-regex, no-magic-numbers, no-prototype-builtins, no-redeclare, no-shadow, no-var, sort-vars*/
import * as $protobuf from "protobufjs/minimal";

// Common aliases
const $Reader = $protobuf.Reader, $Writer = $protobuf.Writer, $util = $protobuf.util;

// Exported root namespace
const $root = $protobuf.roots["default"] || ($protobuf.roots["default"] = {});

export const px = $root.px = (() => {

    /**
     * Namespace px.
     * @exports px
     * @namespace
     */
    const px = {};

    px.vispb = (function() {

        /**
         * Namespace vispb.
         * @memberof px
         * @namespace
         */
        const vispb = {};

        /**
         * PXType enum.
         * @name px.vispb.PXType
         * @enum {number}
         * @property {number} PX_UNKNOWN=0 PX_UNKNOWN value
         * @property {number} PX_BOOLEAN=1 PX_BOOLEAN value
         * @property {number} PX_INT64=2 PX_INT64 value
         * @property {number} PX_FLOAT64=3 PX_FLOAT64 value
         * @property {number} PX_STRING=4 PX_STRING value
         * @property {number} PX_SERVICE=1000 PX_SERVICE value
         * @property {number} PX_POD=1001 PX_POD value
         * @property {number} PX_CONTAINER=1002 PX_CONTAINER value
         * @property {number} PX_NAMESPACE=1003 PX_NAMESPACE value
         * @property {number} PX_NODE=1004 PX_NODE value
         * @property {number} PX_LIST=2000 PX_LIST value
         * @property {number} PX_STRING_LIST=2001 PX_STRING_LIST value
         */
        vispb.PXType = (function() {
            const valuesById = {}, values = Object.create(valuesById);
            values[valuesById[0] = "PX_UNKNOWN"] = 0;
            values[valuesById[1] = "PX_BOOLEAN"] = 1;
            values[valuesById[2] = "PX_INT64"] = 2;
            values[valuesById[3] = "PX_FLOAT64"] = 3;
            values[valuesById[4] = "PX_STRING"] = 4;
            values[valuesById[1000] = "PX_SERVICE"] = 1000;
            values[valuesById[1001] = "PX_POD"] = 1001;
            values[valuesById[1002] = "PX_CONTAINER"] = 1002;
            values[valuesById[1003] = "PX_NAMESPACE"] = 1003;
            values[valuesById[1004] = "PX_NODE"] = 1004;
            values[valuesById[2000] = "PX_LIST"] = 2000;
            values[valuesById[2001] = "PX_STRING_LIST"] = 2001;
            return values;
        })();

        vispb.Vis = (function() {

            /**
             * Properties of a Vis.
             * @memberof px.vispb
             * @interface IVis
             * @property {Array.<px.vispb.Vis.IVariable>|null} [variables] Vis variables
             * @property {Array.<px.vispb.IWidget>|null} [widgets] Vis widgets
             * @property {Array.<px.vispb.Vis.IGlobalFunc>|null} [globalFuncs] Vis globalFuncs
             */

            /**
             * Constructs a new Vis.
             * @memberof px.vispb
             * @classdesc Represents a Vis.
             * @implements IVis
             * @constructor
             * @param {px.vispb.IVis=} [properties] Properties to set
             */
            function Vis(properties) {
                this.variables = [];
                this.widgets = [];
                this.globalFuncs = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Vis variables.
             * @member {Array.<px.vispb.Vis.IVariable>} variables
             * @memberof px.vispb.Vis
             * @instance
             */
            Vis.prototype.variables = $util.emptyArray;

            /**
             * Vis widgets.
             * @member {Array.<px.vispb.IWidget>} widgets
             * @memberof px.vispb.Vis
             * @instance
             */
            Vis.prototype.widgets = $util.emptyArray;

            /**
             * Vis globalFuncs.
             * @member {Array.<px.vispb.Vis.IGlobalFunc>} globalFuncs
             * @memberof px.vispb.Vis
             * @instance
             */
            Vis.prototype.globalFuncs = $util.emptyArray;

            /**
             * Creates a new Vis instance using the specified properties.
             * @function create
             * @memberof px.vispb.Vis
             * @static
             * @param {px.vispb.IVis=} [properties] Properties to set
             * @returns {px.vispb.Vis} Vis instance
             */
            Vis.create = function create(properties) {
                return new Vis(properties);
            };

            /**
             * Encodes the specified Vis message. Does not implicitly {@link px.vispb.Vis.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.Vis
             * @static
             * @param {px.vispb.IVis} message Vis message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Vis.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.variables != null && message.variables.length)
                    for (let i = 0; i < message.variables.length; ++i)
                        $root.px.vispb.Vis.Variable.encode(message.variables[i], writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
                if (message.widgets != null && message.widgets.length)
                    for (let i = 0; i < message.widgets.length; ++i)
                        $root.px.vispb.Widget.encode(message.widgets[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
                if (message.globalFuncs != null && message.globalFuncs.length)
                    for (let i = 0; i < message.globalFuncs.length; ++i)
                        $root.px.vispb.Vis.GlobalFunc.encode(message.globalFuncs[i], writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                return writer;
            };

            /**
             * Encodes the specified Vis message, length delimited. Does not implicitly {@link px.vispb.Vis.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.Vis
             * @static
             * @param {px.vispb.IVis} message Vis message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Vis.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a Vis message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.Vis
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.Vis} Vis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Vis.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Vis();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        if (!(message.variables && message.variables.length))
                            message.variables = [];
                        message.variables.push($root.px.vispb.Vis.Variable.decode(reader, reader.uint32()));
                        break;
                    case 2:
                        if (!(message.widgets && message.widgets.length))
                            message.widgets = [];
                        message.widgets.push($root.px.vispb.Widget.decode(reader, reader.uint32()));
                        break;
                    case 3:
                        if (!(message.globalFuncs && message.globalFuncs.length))
                            message.globalFuncs = [];
                        message.globalFuncs.push($root.px.vispb.Vis.GlobalFunc.decode(reader, reader.uint32()));
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a Vis message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.Vis
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.Vis} Vis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Vis.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a Vis message.
             * @function verify
             * @memberof px.vispb.Vis
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Vis.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.variables != null && message.hasOwnProperty("variables")) {
                    if (!Array.isArray(message.variables))
                        return "variables: array expected";
                    for (let i = 0; i < message.variables.length; ++i) {
                        let error = $root.px.vispb.Vis.Variable.verify(message.variables[i]);
                        if (error)
                            return "variables." + error;
                    }
                }
                if (message.widgets != null && message.hasOwnProperty("widgets")) {
                    if (!Array.isArray(message.widgets))
                        return "widgets: array expected";
                    for (let i = 0; i < message.widgets.length; ++i) {
                        let error = $root.px.vispb.Widget.verify(message.widgets[i]);
                        if (error)
                            return "widgets." + error;
                    }
                }
                if (message.globalFuncs != null && message.hasOwnProperty("globalFuncs")) {
                    if (!Array.isArray(message.globalFuncs))
                        return "globalFuncs: array expected";
                    for (let i = 0; i < message.globalFuncs.length; ++i) {
                        let error = $root.px.vispb.Vis.GlobalFunc.verify(message.globalFuncs[i]);
                        if (error)
                            return "globalFuncs." + error;
                    }
                }
                return null;
            };

            /**
             * Creates a Vis message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.Vis
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.Vis} Vis
             */
            Vis.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.Vis)
                    return object;
                let message = new $root.px.vispb.Vis();
                if (object.variables) {
                    if (!Array.isArray(object.variables))
                        throw TypeError(".px.vispb.Vis.variables: array expected");
                    message.variables = [];
                    for (let i = 0; i < object.variables.length; ++i) {
                        if (typeof object.variables[i] !== "object")
                            throw TypeError(".px.vispb.Vis.variables: object expected");
                        message.variables[i] = $root.px.vispb.Vis.Variable.fromObject(object.variables[i]);
                    }
                }
                if (object.widgets) {
                    if (!Array.isArray(object.widgets))
                        throw TypeError(".px.vispb.Vis.widgets: array expected");
                    message.widgets = [];
                    for (let i = 0; i < object.widgets.length; ++i) {
                        if (typeof object.widgets[i] !== "object")
                            throw TypeError(".px.vispb.Vis.widgets: object expected");
                        message.widgets[i] = $root.px.vispb.Widget.fromObject(object.widgets[i]);
                    }
                }
                if (object.globalFuncs) {
                    if (!Array.isArray(object.globalFuncs))
                        throw TypeError(".px.vispb.Vis.globalFuncs: array expected");
                    message.globalFuncs = [];
                    for (let i = 0; i < object.globalFuncs.length; ++i) {
                        if (typeof object.globalFuncs[i] !== "object")
                            throw TypeError(".px.vispb.Vis.globalFuncs: object expected");
                        message.globalFuncs[i] = $root.px.vispb.Vis.GlobalFunc.fromObject(object.globalFuncs[i]);
                    }
                }
                return message;
            };

            /**
             * Creates a plain object from a Vis message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.Vis
             * @static
             * @param {px.vispb.Vis} message Vis
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Vis.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults) {
                    object.variables = [];
                    object.widgets = [];
                    object.globalFuncs = [];
                }
                if (message.variables && message.variables.length) {
                    object.variables = [];
                    for (let j = 0; j < message.variables.length; ++j)
                        object.variables[j] = $root.px.vispb.Vis.Variable.toObject(message.variables[j], options);
                }
                if (message.widgets && message.widgets.length) {
                    object.widgets = [];
                    for (let j = 0; j < message.widgets.length; ++j)
                        object.widgets[j] = $root.px.vispb.Widget.toObject(message.widgets[j], options);
                }
                if (message.globalFuncs && message.globalFuncs.length) {
                    object.globalFuncs = [];
                    for (let j = 0; j < message.globalFuncs.length; ++j)
                        object.globalFuncs[j] = $root.px.vispb.Vis.GlobalFunc.toObject(message.globalFuncs[j], options);
                }
                return object;
            };

            /**
             * Converts this Vis to JSON.
             * @function toJSON
             * @memberof px.vispb.Vis
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Vis.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            Vis.Variable = (function() {

                /**
                 * Properties of a Variable.
                 * @memberof px.vispb.Vis
                 * @interface IVariable
                 * @property {string|null} [name] Variable name
                 * @property {px.vispb.PXType|null} [type] Variable type
                 * @property {google.protobuf.IStringValue|null} [defaultValue] Variable defaultValue
                 * @property {string|null} [description] Variable description
                 * @property {Array.<string>|null} [validValues] Variable validValues
                 */

                /**
                 * Constructs a new Variable.
                 * @memberof px.vispb.Vis
                 * @classdesc Represents a Variable.
                 * @implements IVariable
                 * @constructor
                 * @param {px.vispb.Vis.IVariable=} [properties] Properties to set
                 */
                function Variable(properties) {
                    this.validValues = [];
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Variable name.
                 * @member {string} name
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 */
                Variable.prototype.name = "";

                /**
                 * Variable type.
                 * @member {px.vispb.PXType} type
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 */
                Variable.prototype.type = 0;

                /**
                 * Variable defaultValue.
                 * @member {google.protobuf.IStringValue|null|undefined} defaultValue
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 */
                Variable.prototype.defaultValue = null;

                /**
                 * Variable description.
                 * @member {string} description
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 */
                Variable.prototype.description = "";

                /**
                 * Variable validValues.
                 * @member {Array.<string>} validValues
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 */
                Variable.prototype.validValues = $util.emptyArray;

                /**
                 * Creates a new Variable instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {px.vispb.Vis.IVariable=} [properties] Properties to set
                 * @returns {px.vispb.Vis.Variable} Variable instance
                 */
                Variable.create = function create(properties) {
                    return new Variable(properties);
                };

                /**
                 * Encodes the specified Variable message. Does not implicitly {@link px.vispb.Vis.Variable.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {px.vispb.Vis.IVariable} message Variable message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Variable.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
                    if (message.type != null && Object.hasOwnProperty.call(message, "type"))
                        writer.uint32(/* id 2, wireType 0 =*/16).int32(message.type);
                    if (message.defaultValue != null && Object.hasOwnProperty.call(message, "defaultValue"))
                        $root.google.protobuf.StringValue.encode(message.defaultValue, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                    if (message.description != null && Object.hasOwnProperty.call(message, "description"))
                        writer.uint32(/* id 4, wireType 2 =*/34).string(message.description);
                    if (message.validValues != null && message.validValues.length)
                        for (let i = 0; i < message.validValues.length; ++i)
                            writer.uint32(/* id 5, wireType 2 =*/42).string(message.validValues[i]);
                    return writer;
                };

                /**
                 * Encodes the specified Variable message, length delimited. Does not implicitly {@link px.vispb.Vis.Variable.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {px.vispb.Vis.IVariable} message Variable message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Variable.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Variable message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Vis.Variable} Variable
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Variable.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Vis.Variable();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.name = reader.string();
                            break;
                        case 2:
                            message.type = reader.int32();
                            break;
                        case 3:
                            message.defaultValue = $root.google.protobuf.StringValue.decode(reader, reader.uint32());
                            break;
                        case 4:
                            message.description = reader.string();
                            break;
                        case 5:
                            if (!(message.validValues && message.validValues.length))
                                message.validValues = [];
                            message.validValues.push(reader.string());
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Variable message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Vis.Variable} Variable
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Variable.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Variable message.
                 * @function verify
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Variable.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.name != null && message.hasOwnProperty("name"))
                        if (!$util.isString(message.name))
                            return "name: string expected";
                    if (message.type != null && message.hasOwnProperty("type"))
                        switch (message.type) {
                        default:
                            return "type: enum value expected";
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                        case 1000:
                        case 1001:
                        case 1002:
                        case 1003:
                        case 1004:
                        case 2000:
                        case 2001:
                            break;
                        }
                    if (message.defaultValue != null && message.hasOwnProperty("defaultValue")) {
                        let error = $root.google.protobuf.StringValue.verify(message.defaultValue);
                        if (error)
                            return "defaultValue." + error;
                    }
                    if (message.description != null && message.hasOwnProperty("description"))
                        if (!$util.isString(message.description))
                            return "description: string expected";
                    if (message.validValues != null && message.hasOwnProperty("validValues")) {
                        if (!Array.isArray(message.validValues))
                            return "validValues: array expected";
                        for (let i = 0; i < message.validValues.length; ++i)
                            if (!$util.isString(message.validValues[i]))
                                return "validValues: string[] expected";
                    }
                    return null;
                };

                /**
                 * Creates a Variable message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Vis.Variable} Variable
                 */
                Variable.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Vis.Variable)
                        return object;
                    let message = new $root.px.vispb.Vis.Variable();
                    if (object.name != null)
                        message.name = String(object.name);
                    switch (object.type) {
                    case "PX_UNKNOWN":
                    case 0:
                        message.type = 0;
                        break;
                    case "PX_BOOLEAN":
                    case 1:
                        message.type = 1;
                        break;
                    case "PX_INT64":
                    case 2:
                        message.type = 2;
                        break;
                    case "PX_FLOAT64":
                    case 3:
                        message.type = 3;
                        break;
                    case "PX_STRING":
                    case 4:
                        message.type = 4;
                        break;
                    case "PX_SERVICE":
                    case 1000:
                        message.type = 1000;
                        break;
                    case "PX_POD":
                    case 1001:
                        message.type = 1001;
                        break;
                    case "PX_CONTAINER":
                    case 1002:
                        message.type = 1002;
                        break;
                    case "PX_NAMESPACE":
                    case 1003:
                        message.type = 1003;
                        break;
                    case "PX_NODE":
                    case 1004:
                        message.type = 1004;
                        break;
                    case "PX_LIST":
                    case 2000:
                        message.type = 2000;
                        break;
                    case "PX_STRING_LIST":
                    case 2001:
                        message.type = 2001;
                        break;
                    }
                    if (object.defaultValue != null) {
                        if (typeof object.defaultValue !== "object")
                            throw TypeError(".px.vispb.Vis.Variable.defaultValue: object expected");
                        message.defaultValue = $root.google.protobuf.StringValue.fromObject(object.defaultValue);
                    }
                    if (object.description != null)
                        message.description = String(object.description);
                    if (object.validValues) {
                        if (!Array.isArray(object.validValues))
                            throw TypeError(".px.vispb.Vis.Variable.validValues: array expected");
                        message.validValues = [];
                        for (let i = 0; i < object.validValues.length; ++i)
                            message.validValues[i] = String(object.validValues[i]);
                    }
                    return message;
                };

                /**
                 * Creates a plain object from a Variable message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Vis.Variable
                 * @static
                 * @param {px.vispb.Vis.Variable} message Variable
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Variable.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.arrays || options.defaults)
                        object.validValues = [];
                    if (options.defaults) {
                        object.name = "";
                        object.type = options.enums === String ? "PX_UNKNOWN" : 0;
                        object.defaultValue = null;
                        object.description = "";
                    }
                    if (message.name != null && message.hasOwnProperty("name"))
                        object.name = message.name;
                    if (message.type != null && message.hasOwnProperty("type"))
                        object.type = options.enums === String ? $root.px.vispb.PXType[message.type] : message.type;
                    if (message.defaultValue != null && message.hasOwnProperty("defaultValue"))
                        object.defaultValue = $root.google.protobuf.StringValue.toObject(message.defaultValue, options);
                    if (message.description != null && message.hasOwnProperty("description"))
                        object.description = message.description;
                    if (message.validValues && message.validValues.length) {
                        object.validValues = [];
                        for (let j = 0; j < message.validValues.length; ++j)
                            object.validValues[j] = message.validValues[j];
                    }
                    return object;
                };

                /**
                 * Converts this Variable to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Vis.Variable
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Variable.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return Variable;
            })();

            Vis.GlobalFunc = (function() {

                /**
                 * Properties of a GlobalFunc.
                 * @memberof px.vispb.Vis
                 * @interface IGlobalFunc
                 * @property {string|null} [outputName] GlobalFunc outputName
                 * @property {px.vispb.Widget.IFunc|null} [func] GlobalFunc func
                 */

                /**
                 * Constructs a new GlobalFunc.
                 * @memberof px.vispb.Vis
                 * @classdesc Represents a GlobalFunc.
                 * @implements IGlobalFunc
                 * @constructor
                 * @param {px.vispb.Vis.IGlobalFunc=} [properties] Properties to set
                 */
                function GlobalFunc(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * GlobalFunc outputName.
                 * @member {string} outputName
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @instance
                 */
                GlobalFunc.prototype.outputName = "";

                /**
                 * GlobalFunc func.
                 * @member {px.vispb.Widget.IFunc|null|undefined} func
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @instance
                 */
                GlobalFunc.prototype.func = null;

                /**
                 * Creates a new GlobalFunc instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {px.vispb.Vis.IGlobalFunc=} [properties] Properties to set
                 * @returns {px.vispb.Vis.GlobalFunc} GlobalFunc instance
                 */
                GlobalFunc.create = function create(properties) {
                    return new GlobalFunc(properties);
                };

                /**
                 * Encodes the specified GlobalFunc message. Does not implicitly {@link px.vispb.Vis.GlobalFunc.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {px.vispb.Vis.IGlobalFunc} message GlobalFunc message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                GlobalFunc.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.outputName != null && Object.hasOwnProperty.call(message, "outputName"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.outputName);
                    if (message.func != null && Object.hasOwnProperty.call(message, "func"))
                        $root.px.vispb.Widget.Func.encode(message.func, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
                    return writer;
                };

                /**
                 * Encodes the specified GlobalFunc message, length delimited. Does not implicitly {@link px.vispb.Vis.GlobalFunc.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {px.vispb.Vis.IGlobalFunc} message GlobalFunc message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                GlobalFunc.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a GlobalFunc message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Vis.GlobalFunc} GlobalFunc
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                GlobalFunc.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Vis.GlobalFunc();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.outputName = reader.string();
                            break;
                        case 2:
                            message.func = $root.px.vispb.Widget.Func.decode(reader, reader.uint32());
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a GlobalFunc message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Vis.GlobalFunc} GlobalFunc
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                GlobalFunc.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a GlobalFunc message.
                 * @function verify
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                GlobalFunc.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.outputName != null && message.hasOwnProperty("outputName"))
                        if (!$util.isString(message.outputName))
                            return "outputName: string expected";
                    if (message.func != null && message.hasOwnProperty("func")) {
                        let error = $root.px.vispb.Widget.Func.verify(message.func);
                        if (error)
                            return "func." + error;
                    }
                    return null;
                };

                /**
                 * Creates a GlobalFunc message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Vis.GlobalFunc} GlobalFunc
                 */
                GlobalFunc.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Vis.GlobalFunc)
                        return object;
                    let message = new $root.px.vispb.Vis.GlobalFunc();
                    if (object.outputName != null)
                        message.outputName = String(object.outputName);
                    if (object.func != null) {
                        if (typeof object.func !== "object")
                            throw TypeError(".px.vispb.Vis.GlobalFunc.func: object expected");
                        message.func = $root.px.vispb.Widget.Func.fromObject(object.func);
                    }
                    return message;
                };

                /**
                 * Creates a plain object from a GlobalFunc message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @static
                 * @param {px.vispb.Vis.GlobalFunc} message GlobalFunc
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                GlobalFunc.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.outputName = "";
                        object.func = null;
                    }
                    if (message.outputName != null && message.hasOwnProperty("outputName"))
                        object.outputName = message.outputName;
                    if (message.func != null && message.hasOwnProperty("func"))
                        object.func = $root.px.vispb.Widget.Func.toObject(message.func, options);
                    return object;
                };

                /**
                 * Converts this GlobalFunc to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Vis.GlobalFunc
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                GlobalFunc.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return GlobalFunc;
            })();

            return Vis;
        })();

        vispb.Widget = (function() {

            /**
             * Properties of a Widget.
             * @memberof px.vispb
             * @interface IWidget
             * @property {string|null} [name] Widget name
             * @property {px.vispb.Widget.IPosition|null} [position] Widget position
             * @property {px.vispb.Widget.IFunc|null} [func] Widget func
             * @property {string|null} [globalFuncOutputName] Widget globalFuncOutputName
             * @property {google.protobuf.IAny|null} [displaySpec] Widget displaySpec
             */

            /**
             * Constructs a new Widget.
             * @memberof px.vispb
             * @classdesc Represents a Widget.
             * @implements IWidget
             * @constructor
             * @param {px.vispb.IWidget=} [properties] Properties to set
             */
            function Widget(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Widget name.
             * @member {string} name
             * @memberof px.vispb.Widget
             * @instance
             */
            Widget.prototype.name = "";

            /**
             * Widget position.
             * @member {px.vispb.Widget.IPosition|null|undefined} position
             * @memberof px.vispb.Widget
             * @instance
             */
            Widget.prototype.position = null;

            /**
             * Widget func.
             * @member {px.vispb.Widget.IFunc|null|undefined} func
             * @memberof px.vispb.Widget
             * @instance
             */
            Widget.prototype.func = null;

            /**
             * Widget globalFuncOutputName.
             * @member {string|null|undefined} globalFuncOutputName
             * @memberof px.vispb.Widget
             * @instance
             */
            Widget.prototype.globalFuncOutputName = null;

            /**
             * Widget displaySpec.
             * @member {google.protobuf.IAny|null|undefined} displaySpec
             * @memberof px.vispb.Widget
             * @instance
             */
            Widget.prototype.displaySpec = null;

            // OneOf field names bound to virtual getters and setters
            let $oneOfFields;

            /**
             * Widget funcOrRef.
             * @member {"func"|"globalFuncOutputName"|undefined} funcOrRef
             * @memberof px.vispb.Widget
             * @instance
             */
            Object.defineProperty(Widget.prototype, "funcOrRef", {
                get: $util.oneOfGetter($oneOfFields = ["func", "globalFuncOutputName"]),
                set: $util.oneOfSetter($oneOfFields)
            });

            /**
             * Creates a new Widget instance using the specified properties.
             * @function create
             * @memberof px.vispb.Widget
             * @static
             * @param {px.vispb.IWidget=} [properties] Properties to set
             * @returns {px.vispb.Widget} Widget instance
             */
            Widget.create = function create(properties) {
                return new Widget(properties);
            };

            /**
             * Encodes the specified Widget message. Does not implicitly {@link px.vispb.Widget.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.Widget
             * @static
             * @param {px.vispb.IWidget} message Widget message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Widget.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
                if (message.position != null && Object.hasOwnProperty.call(message, "position"))
                    $root.px.vispb.Widget.Position.encode(message.position, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
                if (message.func != null && Object.hasOwnProperty.call(message, "func"))
                    $root.px.vispb.Widget.Func.encode(message.func, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                if (message.displaySpec != null && Object.hasOwnProperty.call(message, "displaySpec"))
                    $root.google.protobuf.Any.encode(message.displaySpec, writer.uint32(/* id 4, wireType 2 =*/34).fork()).ldelim();
                if (message.globalFuncOutputName != null && Object.hasOwnProperty.call(message, "globalFuncOutputName"))
                    writer.uint32(/* id 5, wireType 2 =*/42).string(message.globalFuncOutputName);
                return writer;
            };

            /**
             * Encodes the specified Widget message, length delimited. Does not implicitly {@link px.vispb.Widget.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.Widget
             * @static
             * @param {px.vispb.IWidget} message Widget message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Widget.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a Widget message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.Widget
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.Widget} Widget
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Widget.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Widget();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.name = reader.string();
                        break;
                    case 2:
                        message.position = $root.px.vispb.Widget.Position.decode(reader, reader.uint32());
                        break;
                    case 3:
                        message.func = $root.px.vispb.Widget.Func.decode(reader, reader.uint32());
                        break;
                    case 5:
                        message.globalFuncOutputName = reader.string();
                        break;
                    case 4:
                        message.displaySpec = $root.google.protobuf.Any.decode(reader, reader.uint32());
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a Widget message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.Widget
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.Widget} Widget
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Widget.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a Widget message.
             * @function verify
             * @memberof px.vispb.Widget
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Widget.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                let properties = {};
                if (message.name != null && message.hasOwnProperty("name"))
                    if (!$util.isString(message.name))
                        return "name: string expected";
                if (message.position != null && message.hasOwnProperty("position")) {
                    let error = $root.px.vispb.Widget.Position.verify(message.position);
                    if (error)
                        return "position." + error;
                }
                if (message.func != null && message.hasOwnProperty("func")) {
                    properties.funcOrRef = 1;
                    {
                        let error = $root.px.vispb.Widget.Func.verify(message.func);
                        if (error)
                            return "func." + error;
                    }
                }
                if (message.globalFuncOutputName != null && message.hasOwnProperty("globalFuncOutputName")) {
                    if (properties.funcOrRef === 1)
                        return "funcOrRef: multiple values";
                    properties.funcOrRef = 1;
                    if (!$util.isString(message.globalFuncOutputName))
                        return "globalFuncOutputName: string expected";
                }
                if (message.displaySpec != null && message.hasOwnProperty("displaySpec")) {
                    let error = $root.google.protobuf.Any.verify(message.displaySpec);
                    if (error)
                        return "displaySpec." + error;
                }
                return null;
            };

            /**
             * Creates a Widget message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.Widget
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.Widget} Widget
             */
            Widget.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.Widget)
                    return object;
                let message = new $root.px.vispb.Widget();
                if (object.name != null)
                    message.name = String(object.name);
                if (object.position != null) {
                    if (typeof object.position !== "object")
                        throw TypeError(".px.vispb.Widget.position: object expected");
                    message.position = $root.px.vispb.Widget.Position.fromObject(object.position);
                }
                if (object.func != null) {
                    if (typeof object.func !== "object")
                        throw TypeError(".px.vispb.Widget.func: object expected");
                    message.func = $root.px.vispb.Widget.Func.fromObject(object.func);
                }
                if (object.globalFuncOutputName != null)
                    message.globalFuncOutputName = String(object.globalFuncOutputName);
                if (object.displaySpec != null) {
                    if (typeof object.displaySpec !== "object")
                        throw TypeError(".px.vispb.Widget.displaySpec: object expected");
                    message.displaySpec = $root.google.protobuf.Any.fromObject(object.displaySpec);
                }
                return message;
            };

            /**
             * Creates a plain object from a Widget message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.Widget
             * @static
             * @param {px.vispb.Widget} message Widget
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Widget.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.name = "";
                    object.position = null;
                    object.displaySpec = null;
                }
                if (message.name != null && message.hasOwnProperty("name"))
                    object.name = message.name;
                if (message.position != null && message.hasOwnProperty("position"))
                    object.position = $root.px.vispb.Widget.Position.toObject(message.position, options);
                if (message.func != null && message.hasOwnProperty("func")) {
                    object.func = $root.px.vispb.Widget.Func.toObject(message.func, options);
                    if (options.oneofs)
                        object.funcOrRef = "func";
                }
                if (message.displaySpec != null && message.hasOwnProperty("displaySpec"))
                    object.displaySpec = $root.google.protobuf.Any.toObject(message.displaySpec, options);
                if (message.globalFuncOutputName != null && message.hasOwnProperty("globalFuncOutputName")) {
                    object.globalFuncOutputName = message.globalFuncOutputName;
                    if (options.oneofs)
                        object.funcOrRef = "globalFuncOutputName";
                }
                return object;
            };

            /**
             * Converts this Widget to JSON.
             * @function toJSON
             * @memberof px.vispb.Widget
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Widget.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            Widget.Position = (function() {

                /**
                 * Properties of a Position.
                 * @memberof px.vispb.Widget
                 * @interface IPosition
                 * @property {number|null} [x] Position x
                 * @property {number|null} [y] Position y
                 * @property {number|null} [w] Position w
                 * @property {number|null} [h] Position h
                 */

                /**
                 * Constructs a new Position.
                 * @memberof px.vispb.Widget
                 * @classdesc Represents a Position.
                 * @implements IPosition
                 * @constructor
                 * @param {px.vispb.Widget.IPosition=} [properties] Properties to set
                 */
                function Position(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Position x.
                 * @member {number} x
                 * @memberof px.vispb.Widget.Position
                 * @instance
                 */
                Position.prototype.x = 0;

                /**
                 * Position y.
                 * @member {number} y
                 * @memberof px.vispb.Widget.Position
                 * @instance
                 */
                Position.prototype.y = 0;

                /**
                 * Position w.
                 * @member {number} w
                 * @memberof px.vispb.Widget.Position
                 * @instance
                 */
                Position.prototype.w = 0;

                /**
                 * Position h.
                 * @member {number} h
                 * @memberof px.vispb.Widget.Position
                 * @instance
                 */
                Position.prototype.h = 0;

                /**
                 * Creates a new Position instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {px.vispb.Widget.IPosition=} [properties] Properties to set
                 * @returns {px.vispb.Widget.Position} Position instance
                 */
                Position.create = function create(properties) {
                    return new Position(properties);
                };

                /**
                 * Encodes the specified Position message. Does not implicitly {@link px.vispb.Widget.Position.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {px.vispb.Widget.IPosition} message Position message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Position.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.x != null && Object.hasOwnProperty.call(message, "x"))
                        writer.uint32(/* id 1, wireType 0 =*/8).int32(message.x);
                    if (message.y != null && Object.hasOwnProperty.call(message, "y"))
                        writer.uint32(/* id 2, wireType 0 =*/16).int32(message.y);
                    if (message.w != null && Object.hasOwnProperty.call(message, "w"))
                        writer.uint32(/* id 3, wireType 0 =*/24).int32(message.w);
                    if (message.h != null && Object.hasOwnProperty.call(message, "h"))
                        writer.uint32(/* id 4, wireType 0 =*/32).int32(message.h);
                    return writer;
                };

                /**
                 * Encodes the specified Position message, length delimited. Does not implicitly {@link px.vispb.Widget.Position.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {px.vispb.Widget.IPosition} message Position message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Position.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Position message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Widget.Position} Position
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Position.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Widget.Position();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.x = reader.int32();
                            break;
                        case 2:
                            message.y = reader.int32();
                            break;
                        case 3:
                            message.w = reader.int32();
                            break;
                        case 4:
                            message.h = reader.int32();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Position message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Widget.Position} Position
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Position.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Position message.
                 * @function verify
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Position.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.x != null && message.hasOwnProperty("x"))
                        if (!$util.isInteger(message.x))
                            return "x: integer expected";
                    if (message.y != null && message.hasOwnProperty("y"))
                        if (!$util.isInteger(message.y))
                            return "y: integer expected";
                    if (message.w != null && message.hasOwnProperty("w"))
                        if (!$util.isInteger(message.w))
                            return "w: integer expected";
                    if (message.h != null && message.hasOwnProperty("h"))
                        if (!$util.isInteger(message.h))
                            return "h: integer expected";
                    return null;
                };

                /**
                 * Creates a Position message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Widget.Position} Position
                 */
                Position.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Widget.Position)
                        return object;
                    let message = new $root.px.vispb.Widget.Position();
                    if (object.x != null)
                        message.x = object.x | 0;
                    if (object.y != null)
                        message.y = object.y | 0;
                    if (object.w != null)
                        message.w = object.w | 0;
                    if (object.h != null)
                        message.h = object.h | 0;
                    return message;
                };

                /**
                 * Creates a plain object from a Position message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Widget.Position
                 * @static
                 * @param {px.vispb.Widget.Position} message Position
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Position.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.x = 0;
                        object.y = 0;
                        object.w = 0;
                        object.h = 0;
                    }
                    if (message.x != null && message.hasOwnProperty("x"))
                        object.x = message.x;
                    if (message.y != null && message.hasOwnProperty("y"))
                        object.y = message.y;
                    if (message.w != null && message.hasOwnProperty("w"))
                        object.w = message.w;
                    if (message.h != null && message.hasOwnProperty("h"))
                        object.h = message.h;
                    return object;
                };

                /**
                 * Converts this Position to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Widget.Position
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Position.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return Position;
            })();

            Widget.Func = (function() {

                /**
                 * Properties of a Func.
                 * @memberof px.vispb.Widget
                 * @interface IFunc
                 * @property {string|null} [name] Func name
                 * @property {Array.<px.vispb.Widget.Func.IFuncArg>|null} [args] Func args
                 */

                /**
                 * Constructs a new Func.
                 * @memberof px.vispb.Widget
                 * @classdesc Represents a Func.
                 * @implements IFunc
                 * @constructor
                 * @param {px.vispb.Widget.IFunc=} [properties] Properties to set
                 */
                function Func(properties) {
                    this.args = [];
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Func name.
                 * @member {string} name
                 * @memberof px.vispb.Widget.Func
                 * @instance
                 */
                Func.prototype.name = "";

                /**
                 * Func args.
                 * @member {Array.<px.vispb.Widget.Func.IFuncArg>} args
                 * @memberof px.vispb.Widget.Func
                 * @instance
                 */
                Func.prototype.args = $util.emptyArray;

                /**
                 * Creates a new Func instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {px.vispb.Widget.IFunc=} [properties] Properties to set
                 * @returns {px.vispb.Widget.Func} Func instance
                 */
                Func.create = function create(properties) {
                    return new Func(properties);
                };

                /**
                 * Encodes the specified Func message. Does not implicitly {@link px.vispb.Widget.Func.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {px.vispb.Widget.IFunc} message Func message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Func.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
                    if (message.args != null && message.args.length)
                        for (let i = 0; i < message.args.length; ++i)
                            $root.px.vispb.Widget.Func.FuncArg.encode(message.args[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
                    return writer;
                };

                /**
                 * Encodes the specified Func message, length delimited. Does not implicitly {@link px.vispb.Widget.Func.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {px.vispb.Widget.IFunc} message Func message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Func.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Func message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Widget.Func} Func
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Func.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Widget.Func();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.name = reader.string();
                            break;
                        case 2:
                            if (!(message.args && message.args.length))
                                message.args = [];
                            message.args.push($root.px.vispb.Widget.Func.FuncArg.decode(reader, reader.uint32()));
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Func message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Widget.Func} Func
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Func.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Func message.
                 * @function verify
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Func.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.name != null && message.hasOwnProperty("name"))
                        if (!$util.isString(message.name))
                            return "name: string expected";
                    if (message.args != null && message.hasOwnProperty("args")) {
                        if (!Array.isArray(message.args))
                            return "args: array expected";
                        for (let i = 0; i < message.args.length; ++i) {
                            let error = $root.px.vispb.Widget.Func.FuncArg.verify(message.args[i]);
                            if (error)
                                return "args." + error;
                        }
                    }
                    return null;
                };

                /**
                 * Creates a Func message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Widget.Func} Func
                 */
                Func.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Widget.Func)
                        return object;
                    let message = new $root.px.vispb.Widget.Func();
                    if (object.name != null)
                        message.name = String(object.name);
                    if (object.args) {
                        if (!Array.isArray(object.args))
                            throw TypeError(".px.vispb.Widget.Func.args: array expected");
                        message.args = [];
                        for (let i = 0; i < object.args.length; ++i) {
                            if (typeof object.args[i] !== "object")
                                throw TypeError(".px.vispb.Widget.Func.args: object expected");
                            message.args[i] = $root.px.vispb.Widget.Func.FuncArg.fromObject(object.args[i]);
                        }
                    }
                    return message;
                };

                /**
                 * Creates a plain object from a Func message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Widget.Func
                 * @static
                 * @param {px.vispb.Widget.Func} message Func
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Func.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.arrays || options.defaults)
                        object.args = [];
                    if (options.defaults)
                        object.name = "";
                    if (message.name != null && message.hasOwnProperty("name"))
                        object.name = message.name;
                    if (message.args && message.args.length) {
                        object.args = [];
                        for (let j = 0; j < message.args.length; ++j)
                            object.args[j] = $root.px.vispb.Widget.Func.FuncArg.toObject(message.args[j], options);
                    }
                    return object;
                };

                /**
                 * Converts this Func to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Widget.Func
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Func.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                Func.FuncArg = (function() {

                    /**
                     * Properties of a FuncArg.
                     * @memberof px.vispb.Widget.Func
                     * @interface IFuncArg
                     * @property {string|null} [name] FuncArg name
                     * @property {string|null} [value] FuncArg value
                     * @property {string|null} [variable] FuncArg variable
                     */

                    /**
                     * Constructs a new FuncArg.
                     * @memberof px.vispb.Widget.Func
                     * @classdesc Represents a FuncArg.
                     * @implements IFuncArg
                     * @constructor
                     * @param {px.vispb.Widget.Func.IFuncArg=} [properties] Properties to set
                     */
                    function FuncArg(properties) {
                        if (properties)
                            for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null)
                                    this[keys[i]] = properties[keys[i]];
                    }

                    /**
                     * FuncArg name.
                     * @member {string} name
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @instance
                     */
                    FuncArg.prototype.name = "";

                    /**
                     * FuncArg value.
                     * @member {string|null|undefined} value
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @instance
                     */
                    FuncArg.prototype.value = null;

                    /**
                     * FuncArg variable.
                     * @member {string|null|undefined} variable
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @instance
                     */
                    FuncArg.prototype.variable = null;

                    // OneOf field names bound to virtual getters and setters
                    let $oneOfFields;

                    /**
                     * FuncArg input.
                     * @member {"value"|"variable"|undefined} input
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @instance
                     */
                    Object.defineProperty(FuncArg.prototype, "input", {
                        get: $util.oneOfGetter($oneOfFields = ["value", "variable"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });

                    /**
                     * Creates a new FuncArg instance using the specified properties.
                     * @function create
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {px.vispb.Widget.Func.IFuncArg=} [properties] Properties to set
                     * @returns {px.vispb.Widget.Func.FuncArg} FuncArg instance
                     */
                    FuncArg.create = function create(properties) {
                        return new FuncArg(properties);
                    };

                    /**
                     * Encodes the specified FuncArg message. Does not implicitly {@link px.vispb.Widget.Func.FuncArg.verify|verify} messages.
                     * @function encode
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {px.vispb.Widget.Func.IFuncArg} message FuncArg message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    FuncArg.encode = function encode(message, writer) {
                        if (!writer)
                            writer = $Writer.create();
                        if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
                        if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.value);
                        if (message.variable != null && Object.hasOwnProperty.call(message, "variable"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.variable);
                        return writer;
                    };

                    /**
                     * Encodes the specified FuncArg message, length delimited. Does not implicitly {@link px.vispb.Widget.Func.FuncArg.verify|verify} messages.
                     * @function encodeDelimited
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {px.vispb.Widget.Func.IFuncArg} message FuncArg message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    FuncArg.encodeDelimited = function encodeDelimited(message, writer) {
                        return this.encode(message, writer).ldelim();
                    };

                    /**
                     * Decodes a FuncArg message from the specified reader or buffer.
                     * @function decode
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {px.vispb.Widget.Func.FuncArg} FuncArg
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    FuncArg.decode = function decode(reader, length) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Widget.Func.FuncArg();
                        while (reader.pos < end) {
                            let tag = reader.uint32();
                            switch (tag >>> 3) {
                            case 1:
                                message.name = reader.string();
                                break;
                            case 2:
                                message.value = reader.string();
                                break;
                            case 3:
                                message.variable = reader.string();
                                break;
                            default:
                                reader.skipType(tag & 7);
                                break;
                            }
                        }
                        return message;
                    };

                    /**
                     * Decodes a FuncArg message from the specified reader or buffer, length delimited.
                     * @function decodeDelimited
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @returns {px.vispb.Widget.Func.FuncArg} FuncArg
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    FuncArg.decodeDelimited = function decodeDelimited(reader) {
                        if (!(reader instanceof $Reader))
                            reader = new $Reader(reader);
                        return this.decode(reader, reader.uint32());
                    };

                    /**
                     * Verifies a FuncArg message.
                     * @function verify
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    FuncArg.verify = function verify(message) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        let properties = {};
                        if (message.name != null && message.hasOwnProperty("name"))
                            if (!$util.isString(message.name))
                                return "name: string expected";
                        if (message.value != null && message.hasOwnProperty("value")) {
                            properties.input = 1;
                            if (!$util.isString(message.value))
                                return "value: string expected";
                        }
                        if (message.variable != null && message.hasOwnProperty("variable")) {
                            if (properties.input === 1)
                                return "input: multiple values";
                            properties.input = 1;
                            if (!$util.isString(message.variable))
                                return "variable: string expected";
                        }
                        return null;
                    };

                    /**
                     * Creates a FuncArg message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {px.vispb.Widget.Func.FuncArg} FuncArg
                     */
                    FuncArg.fromObject = function fromObject(object) {
                        if (object instanceof $root.px.vispb.Widget.Func.FuncArg)
                            return object;
                        let message = new $root.px.vispb.Widget.Func.FuncArg();
                        if (object.name != null)
                            message.name = String(object.name);
                        if (object.value != null)
                            message.value = String(object.value);
                        if (object.variable != null)
                            message.variable = String(object.variable);
                        return message;
                    };

                    /**
                     * Creates a plain object from a FuncArg message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @static
                     * @param {px.vispb.Widget.Func.FuncArg} message FuncArg
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    FuncArg.toObject = function toObject(message, options) {
                        if (!options)
                            options = {};
                        let object = {};
                        if (options.defaults)
                            object.name = "";
                        if (message.name != null && message.hasOwnProperty("name"))
                            object.name = message.name;
                        if (message.value != null && message.hasOwnProperty("value")) {
                            object.value = message.value;
                            if (options.oneofs)
                                object.input = "value";
                        }
                        if (message.variable != null && message.hasOwnProperty("variable")) {
                            object.variable = message.variable;
                            if (options.oneofs)
                                object.input = "variable";
                        }
                        return object;
                    };

                    /**
                     * Converts this FuncArg to JSON.
                     * @function toJSON
                     * @memberof px.vispb.Widget.Func.FuncArg
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    FuncArg.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };

                    return FuncArg;
                })();

                return Func;
            })();

            return Widget;
        })();

        vispb.Axis = (function() {

            /**
             * Properties of an Axis.
             * @memberof px.vispb
             * @interface IAxis
             * @property {string|null} [label] Axis label
             */

            /**
             * Constructs a new Axis.
             * @memberof px.vispb
             * @classdesc Represents an Axis.
             * @implements IAxis
             * @constructor
             * @param {px.vispb.IAxis=} [properties] Properties to set
             */
            function Axis(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Axis label.
             * @member {string} label
             * @memberof px.vispb.Axis
             * @instance
             */
            Axis.prototype.label = "";

            /**
             * Creates a new Axis instance using the specified properties.
             * @function create
             * @memberof px.vispb.Axis
             * @static
             * @param {px.vispb.IAxis=} [properties] Properties to set
             * @returns {px.vispb.Axis} Axis instance
             */
            Axis.create = function create(properties) {
                return new Axis(properties);
            };

            /**
             * Encodes the specified Axis message. Does not implicitly {@link px.vispb.Axis.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.Axis
             * @static
             * @param {px.vispb.IAxis} message Axis message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Axis.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.label);
                return writer;
            };

            /**
             * Encodes the specified Axis message, length delimited. Does not implicitly {@link px.vispb.Axis.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.Axis
             * @static
             * @param {px.vispb.IAxis} message Axis message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Axis.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes an Axis message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.Axis
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.Axis} Axis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Axis.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Axis();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.label = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes an Axis message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.Axis
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.Axis} Axis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Axis.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies an Axis message.
             * @function verify
             * @memberof px.vispb.Axis
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Axis.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.label != null && message.hasOwnProperty("label"))
                    if (!$util.isString(message.label))
                        return "label: string expected";
                return null;
            };

            /**
             * Creates an Axis message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.Axis
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.Axis} Axis
             */
            Axis.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.Axis)
                    return object;
                let message = new $root.px.vispb.Axis();
                if (object.label != null)
                    message.label = String(object.label);
                return message;
            };

            /**
             * Creates a plain object from an Axis message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.Axis
             * @static
             * @param {px.vispb.Axis} message Axis
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Axis.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.label = "";
                if (message.label != null && message.hasOwnProperty("label"))
                    object.label = message.label;
                return object;
            };

            /**
             * Converts this Axis to JSON.
             * @function toJSON
             * @memberof px.vispb.Axis
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Axis.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return Axis;
        })();

        vispb.BarChart = (function() {

            /**
             * Properties of a BarChart.
             * @memberof px.vispb
             * @interface IBarChart
             * @property {px.vispb.BarChart.IBar|null} [bar] BarChart bar
             * @property {string|null} [title] BarChart title
             * @property {px.vispb.IAxis|null} [xAxis] BarChart xAxis
             * @property {px.vispb.IAxis|null} [yAxis] BarChart yAxis
             */

            /**
             * Constructs a new BarChart.
             * @memberof px.vispb
             * @classdesc Represents a BarChart.
             * @implements IBarChart
             * @constructor
             * @param {px.vispb.IBarChart=} [properties] Properties to set
             */
            function BarChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * BarChart bar.
             * @member {px.vispb.BarChart.IBar|null|undefined} bar
             * @memberof px.vispb.BarChart
             * @instance
             */
            BarChart.prototype.bar = null;

            /**
             * BarChart title.
             * @member {string} title
             * @memberof px.vispb.BarChart
             * @instance
             */
            BarChart.prototype.title = "";

            /**
             * BarChart xAxis.
             * @member {px.vispb.IAxis|null|undefined} xAxis
             * @memberof px.vispb.BarChart
             * @instance
             */
            BarChart.prototype.xAxis = null;

            /**
             * BarChart yAxis.
             * @member {px.vispb.IAxis|null|undefined} yAxis
             * @memberof px.vispb.BarChart
             * @instance
             */
            BarChart.prototype.yAxis = null;

            /**
             * Creates a new BarChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.BarChart
             * @static
             * @param {px.vispb.IBarChart=} [properties] Properties to set
             * @returns {px.vispb.BarChart} BarChart instance
             */
            BarChart.create = function create(properties) {
                return new BarChart(properties);
            };

            /**
             * Encodes the specified BarChart message. Does not implicitly {@link px.vispb.BarChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.BarChart
             * @static
             * @param {px.vispb.IBarChart} message BarChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BarChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.bar != null && Object.hasOwnProperty.call(message, "bar"))
                    $root.px.vispb.BarChart.Bar.encode(message.bar, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                if (message.xAxis != null && Object.hasOwnProperty.call(message, "xAxis"))
                    $root.px.vispb.Axis.encode(message.xAxis, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                if (message.yAxis != null && Object.hasOwnProperty.call(message, "yAxis"))
                    $root.px.vispb.Axis.encode(message.yAxis, writer.uint32(/* id 4, wireType 2 =*/34).fork()).ldelim();
                return writer;
            };

            /**
             * Encodes the specified BarChart message, length delimited. Does not implicitly {@link px.vispb.BarChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.BarChart
             * @static
             * @param {px.vispb.IBarChart} message BarChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BarChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a BarChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.BarChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.BarChart} BarChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BarChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.BarChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.bar = $root.px.vispb.BarChart.Bar.decode(reader, reader.uint32());
                        break;
                    case 2:
                        message.title = reader.string();
                        break;
                    case 3:
                        message.xAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    case 4:
                        message.yAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a BarChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.BarChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.BarChart} BarChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BarChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a BarChart message.
             * @function verify
             * @memberof px.vispb.BarChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            BarChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.bar != null && message.hasOwnProperty("bar")) {
                    let error = $root.px.vispb.BarChart.Bar.verify(message.bar);
                    if (error)
                        return "bar." + error;
                }
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                if (message.xAxis != null && message.hasOwnProperty("xAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.xAxis);
                    if (error)
                        return "xAxis." + error;
                }
                if (message.yAxis != null && message.hasOwnProperty("yAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.yAxis);
                    if (error)
                        return "yAxis." + error;
                }
                return null;
            };

            /**
             * Creates a BarChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.BarChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.BarChart} BarChart
             */
            BarChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.BarChart)
                    return object;
                let message = new $root.px.vispb.BarChart();
                if (object.bar != null) {
                    if (typeof object.bar !== "object")
                        throw TypeError(".px.vispb.BarChart.bar: object expected");
                    message.bar = $root.px.vispb.BarChart.Bar.fromObject(object.bar);
                }
                if (object.title != null)
                    message.title = String(object.title);
                if (object.xAxis != null) {
                    if (typeof object.xAxis !== "object")
                        throw TypeError(".px.vispb.BarChart.xAxis: object expected");
                    message.xAxis = $root.px.vispb.Axis.fromObject(object.xAxis);
                }
                if (object.yAxis != null) {
                    if (typeof object.yAxis !== "object")
                        throw TypeError(".px.vispb.BarChart.yAxis: object expected");
                    message.yAxis = $root.px.vispb.Axis.fromObject(object.yAxis);
                }
                return message;
            };

            /**
             * Creates a plain object from a BarChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.BarChart
             * @static
             * @param {px.vispb.BarChart} message BarChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            BarChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.bar = null;
                    object.title = "";
                    object.xAxis = null;
                    object.yAxis = null;
                }
                if (message.bar != null && message.hasOwnProperty("bar"))
                    object.bar = $root.px.vispb.BarChart.Bar.toObject(message.bar, options);
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                if (message.xAxis != null && message.hasOwnProperty("xAxis"))
                    object.xAxis = $root.px.vispb.Axis.toObject(message.xAxis, options);
                if (message.yAxis != null && message.hasOwnProperty("yAxis"))
                    object.yAxis = $root.px.vispb.Axis.toObject(message.yAxis, options);
                return object;
            };

            /**
             * Converts this BarChart to JSON.
             * @function toJSON
             * @memberof px.vispb.BarChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            BarChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            BarChart.Bar = (function() {

                /**
                 * Properties of a Bar.
                 * @memberof px.vispb.BarChart
                 * @interface IBar
                 * @property {string|null} [value] Bar value
                 * @property {string|null} [label] Bar label
                 * @property {string|null} [stackBy] Bar stackBy
                 * @property {string|null} [groupBy] Bar groupBy
                 * @property {boolean|null} [horizontal] Bar horizontal
                 */

                /**
                 * Constructs a new Bar.
                 * @memberof px.vispb.BarChart
                 * @classdesc Represents a Bar.
                 * @implements IBar
                 * @constructor
                 * @param {px.vispb.BarChart.IBar=} [properties] Properties to set
                 */
                function Bar(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Bar value.
                 * @member {string} value
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 */
                Bar.prototype.value = "";

                /**
                 * Bar label.
                 * @member {string} label
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 */
                Bar.prototype.label = "";

                /**
                 * Bar stackBy.
                 * @member {string} stackBy
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 */
                Bar.prototype.stackBy = "";

                /**
                 * Bar groupBy.
                 * @member {string} groupBy
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 */
                Bar.prototype.groupBy = "";

                /**
                 * Bar horizontal.
                 * @member {boolean} horizontal
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 */
                Bar.prototype.horizontal = false;

                /**
                 * Creates a new Bar instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {px.vispb.BarChart.IBar=} [properties] Properties to set
                 * @returns {px.vispb.BarChart.Bar} Bar instance
                 */
                Bar.create = function create(properties) {
                    return new Bar(properties);
                };

                /**
                 * Encodes the specified Bar message. Does not implicitly {@link px.vispb.BarChart.Bar.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {px.vispb.BarChart.IBar} message Bar message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Bar.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                    if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                        writer.uint32(/* id 2, wireType 2 =*/18).string(message.label);
                    if (message.stackBy != null && Object.hasOwnProperty.call(message, "stackBy"))
                        writer.uint32(/* id 3, wireType 2 =*/26).string(message.stackBy);
                    if (message.groupBy != null && Object.hasOwnProperty.call(message, "groupBy"))
                        writer.uint32(/* id 4, wireType 2 =*/34).string(message.groupBy);
                    if (message.horizontal != null && Object.hasOwnProperty.call(message, "horizontal"))
                        writer.uint32(/* id 5, wireType 0 =*/40).bool(message.horizontal);
                    return writer;
                };

                /**
                 * Encodes the specified Bar message, length delimited. Does not implicitly {@link px.vispb.BarChart.Bar.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {px.vispb.BarChart.IBar} message Bar message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Bar.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Bar message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.BarChart.Bar} Bar
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Bar.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.BarChart.Bar();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.value = reader.string();
                            break;
                        case 2:
                            message.label = reader.string();
                            break;
                        case 3:
                            message.stackBy = reader.string();
                            break;
                        case 4:
                            message.groupBy = reader.string();
                            break;
                        case 5:
                            message.horizontal = reader.bool();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Bar message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.BarChart.Bar} Bar
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Bar.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Bar message.
                 * @function verify
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Bar.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.value != null && message.hasOwnProperty("value"))
                        if (!$util.isString(message.value))
                            return "value: string expected";
                    if (message.label != null && message.hasOwnProperty("label"))
                        if (!$util.isString(message.label))
                            return "label: string expected";
                    if (message.stackBy != null && message.hasOwnProperty("stackBy"))
                        if (!$util.isString(message.stackBy))
                            return "stackBy: string expected";
                    if (message.groupBy != null && message.hasOwnProperty("groupBy"))
                        if (!$util.isString(message.groupBy))
                            return "groupBy: string expected";
                    if (message.horizontal != null && message.hasOwnProperty("horizontal"))
                        if (typeof message.horizontal !== "boolean")
                            return "horizontal: boolean expected";
                    return null;
                };

                /**
                 * Creates a Bar message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.BarChart.Bar} Bar
                 */
                Bar.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.BarChart.Bar)
                        return object;
                    let message = new $root.px.vispb.BarChart.Bar();
                    if (object.value != null)
                        message.value = String(object.value);
                    if (object.label != null)
                        message.label = String(object.label);
                    if (object.stackBy != null)
                        message.stackBy = String(object.stackBy);
                    if (object.groupBy != null)
                        message.groupBy = String(object.groupBy);
                    if (object.horizontal != null)
                        message.horizontal = Boolean(object.horizontal);
                    return message;
                };

                /**
                 * Creates a plain object from a Bar message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.BarChart.Bar
                 * @static
                 * @param {px.vispb.BarChart.Bar} message Bar
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Bar.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.value = "";
                        object.label = "";
                        object.stackBy = "";
                        object.groupBy = "";
                        object.horizontal = false;
                    }
                    if (message.value != null && message.hasOwnProperty("value"))
                        object.value = message.value;
                    if (message.label != null && message.hasOwnProperty("label"))
                        object.label = message.label;
                    if (message.stackBy != null && message.hasOwnProperty("stackBy"))
                        object.stackBy = message.stackBy;
                    if (message.groupBy != null && message.hasOwnProperty("groupBy"))
                        object.groupBy = message.groupBy;
                    if (message.horizontal != null && message.hasOwnProperty("horizontal"))
                        object.horizontal = message.horizontal;
                    return object;
                };

                /**
                 * Converts this Bar to JSON.
                 * @function toJSON
                 * @memberof px.vispb.BarChart.Bar
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Bar.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return Bar;
            })();

            return BarChart;
        })();

        vispb.PieChart = (function() {

            /**
             * Properties of a PieChart.
             * @memberof px.vispb
             * @interface IPieChart
             * @property {string|null} [label] PieChart label
             * @property {string|null} [value] PieChart value
             * @property {string|null} [title] PieChart title
             */

            /**
             * Constructs a new PieChart.
             * @memberof px.vispb
             * @classdesc Represents a PieChart.
             * @implements IPieChart
             * @constructor
             * @param {px.vispb.IPieChart=} [properties] Properties to set
             */
            function PieChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * PieChart label.
             * @member {string} label
             * @memberof px.vispb.PieChart
             * @instance
             */
            PieChart.prototype.label = "";

            /**
             * PieChart value.
             * @member {string} value
             * @memberof px.vispb.PieChart
             * @instance
             */
            PieChart.prototype.value = "";

            /**
             * PieChart title.
             * @member {string} title
             * @memberof px.vispb.PieChart
             * @instance
             */
            PieChart.prototype.title = "";

            /**
             * Creates a new PieChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.PieChart
             * @static
             * @param {px.vispb.IPieChart=} [properties] Properties to set
             * @returns {px.vispb.PieChart} PieChart instance
             */
            PieChart.create = function create(properties) {
                return new PieChart(properties);
            };

            /**
             * Encodes the specified PieChart message. Does not implicitly {@link px.vispb.PieChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.PieChart
             * @static
             * @param {px.vispb.IPieChart} message PieChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            PieChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.label);
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.value);
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.title);
                return writer;
            };

            /**
             * Encodes the specified PieChart message, length delimited. Does not implicitly {@link px.vispb.PieChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.PieChart
             * @static
             * @param {px.vispb.IPieChart} message PieChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            PieChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a PieChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.PieChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.PieChart} PieChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            PieChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.PieChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.label = reader.string();
                        break;
                    case 2:
                        message.value = reader.string();
                        break;
                    case 3:
                        message.title = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a PieChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.PieChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.PieChart} PieChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            PieChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a PieChart message.
             * @function verify
             * @memberof px.vispb.PieChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            PieChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.label != null && message.hasOwnProperty("label"))
                    if (!$util.isString(message.label))
                        return "label: string expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isString(message.value))
                        return "value: string expected";
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                return null;
            };

            /**
             * Creates a PieChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.PieChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.PieChart} PieChart
             */
            PieChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.PieChart)
                    return object;
                let message = new $root.px.vispb.PieChart();
                if (object.label != null)
                    message.label = String(object.label);
                if (object.value != null)
                    message.value = String(object.value);
                if (object.title != null)
                    message.title = String(object.title);
                return message;
            };

            /**
             * Creates a plain object from a PieChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.PieChart
             * @static
             * @param {px.vispb.PieChart} message PieChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            PieChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.label = "";
                    object.value = "";
                    object.title = "";
                }
                if (message.label != null && message.hasOwnProperty("label"))
                    object.label = message.label;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                return object;
            };

            /**
             * Converts this PieChart to JSON.
             * @function toJSON
             * @memberof px.vispb.PieChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            PieChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return PieChart;
        })();

        vispb.HistogramChart = (function() {

            /**
             * Properties of a HistogramChart.
             * @memberof px.vispb
             * @interface IHistogramChart
             * @property {px.vispb.HistogramChart.IHistogram|null} [histogram] HistogramChart histogram
             * @property {string|null} [title] HistogramChart title
             * @property {px.vispb.IAxis|null} [xAxis] HistogramChart xAxis
             * @property {px.vispb.IAxis|null} [yAxis] HistogramChart yAxis
             */

            /**
             * Constructs a new HistogramChart.
             * @memberof px.vispb
             * @classdesc Represents a HistogramChart.
             * @implements IHistogramChart
             * @constructor
             * @param {px.vispb.IHistogramChart=} [properties] Properties to set
             */
            function HistogramChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * HistogramChart histogram.
             * @member {px.vispb.HistogramChart.IHistogram|null|undefined} histogram
             * @memberof px.vispb.HistogramChart
             * @instance
             */
            HistogramChart.prototype.histogram = null;

            /**
             * HistogramChart title.
             * @member {string} title
             * @memberof px.vispb.HistogramChart
             * @instance
             */
            HistogramChart.prototype.title = "";

            /**
             * HistogramChart xAxis.
             * @member {px.vispb.IAxis|null|undefined} xAxis
             * @memberof px.vispb.HistogramChart
             * @instance
             */
            HistogramChart.prototype.xAxis = null;

            /**
             * HistogramChart yAxis.
             * @member {px.vispb.IAxis|null|undefined} yAxis
             * @memberof px.vispb.HistogramChart
             * @instance
             */
            HistogramChart.prototype.yAxis = null;

            /**
             * Creates a new HistogramChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {px.vispb.IHistogramChart=} [properties] Properties to set
             * @returns {px.vispb.HistogramChart} HistogramChart instance
             */
            HistogramChart.create = function create(properties) {
                return new HistogramChart(properties);
            };

            /**
             * Encodes the specified HistogramChart message. Does not implicitly {@link px.vispb.HistogramChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {px.vispb.IHistogramChart} message HistogramChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            HistogramChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.histogram != null && Object.hasOwnProperty.call(message, "histogram"))
                    $root.px.vispb.HistogramChart.Histogram.encode(message.histogram, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                if (message.xAxis != null && Object.hasOwnProperty.call(message, "xAxis"))
                    $root.px.vispb.Axis.encode(message.xAxis, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                if (message.yAxis != null && Object.hasOwnProperty.call(message, "yAxis"))
                    $root.px.vispb.Axis.encode(message.yAxis, writer.uint32(/* id 4, wireType 2 =*/34).fork()).ldelim();
                return writer;
            };

            /**
             * Encodes the specified HistogramChart message, length delimited. Does not implicitly {@link px.vispb.HistogramChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {px.vispb.IHistogramChart} message HistogramChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            HistogramChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a HistogramChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.HistogramChart} HistogramChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            HistogramChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.HistogramChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.histogram = $root.px.vispb.HistogramChart.Histogram.decode(reader, reader.uint32());
                        break;
                    case 2:
                        message.title = reader.string();
                        break;
                    case 3:
                        message.xAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    case 4:
                        message.yAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a HistogramChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.HistogramChart} HistogramChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            HistogramChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a HistogramChart message.
             * @function verify
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            HistogramChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.histogram != null && message.hasOwnProperty("histogram")) {
                    let error = $root.px.vispb.HistogramChart.Histogram.verify(message.histogram);
                    if (error)
                        return "histogram." + error;
                }
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                if (message.xAxis != null && message.hasOwnProperty("xAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.xAxis);
                    if (error)
                        return "xAxis." + error;
                }
                if (message.yAxis != null && message.hasOwnProperty("yAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.yAxis);
                    if (error)
                        return "yAxis." + error;
                }
                return null;
            };

            /**
             * Creates a HistogramChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.HistogramChart} HistogramChart
             */
            HistogramChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.HistogramChart)
                    return object;
                let message = new $root.px.vispb.HistogramChart();
                if (object.histogram != null) {
                    if (typeof object.histogram !== "object")
                        throw TypeError(".px.vispb.HistogramChart.histogram: object expected");
                    message.histogram = $root.px.vispb.HistogramChart.Histogram.fromObject(object.histogram);
                }
                if (object.title != null)
                    message.title = String(object.title);
                if (object.xAxis != null) {
                    if (typeof object.xAxis !== "object")
                        throw TypeError(".px.vispb.HistogramChart.xAxis: object expected");
                    message.xAxis = $root.px.vispb.Axis.fromObject(object.xAxis);
                }
                if (object.yAxis != null) {
                    if (typeof object.yAxis !== "object")
                        throw TypeError(".px.vispb.HistogramChart.yAxis: object expected");
                    message.yAxis = $root.px.vispb.Axis.fromObject(object.yAxis);
                }
                return message;
            };

            /**
             * Creates a plain object from a HistogramChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.HistogramChart
             * @static
             * @param {px.vispb.HistogramChart} message HistogramChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            HistogramChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.histogram = null;
                    object.title = "";
                    object.xAxis = null;
                    object.yAxis = null;
                }
                if (message.histogram != null && message.hasOwnProperty("histogram"))
                    object.histogram = $root.px.vispb.HistogramChart.Histogram.toObject(message.histogram, options);
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                if (message.xAxis != null && message.hasOwnProperty("xAxis"))
                    object.xAxis = $root.px.vispb.Axis.toObject(message.xAxis, options);
                if (message.yAxis != null && message.hasOwnProperty("yAxis"))
                    object.yAxis = $root.px.vispb.Axis.toObject(message.yAxis, options);
                return object;
            };

            /**
             * Converts this HistogramChart to JSON.
             * @function toJSON
             * @memberof px.vispb.HistogramChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            HistogramChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            HistogramChart.Histogram = (function() {

                /**
                 * Properties of a Histogram.
                 * @memberof px.vispb.HistogramChart
                 * @interface IHistogram
                 * @property {string|null} [value] Histogram value
                 * @property {number|Long|null} [maxbins] Histogram maxbins
                 * @property {number|null} [minstep] Histogram minstep
                 * @property {boolean|null} [horizontal] Histogram horizontal
                 * @property {string|null} [prebinCount] Histogram prebinCount
                 */

                /**
                 * Constructs a new Histogram.
                 * @memberof px.vispb.HistogramChart
                 * @classdesc Represents a Histogram.
                 * @implements IHistogram
                 * @constructor
                 * @param {px.vispb.HistogramChart.IHistogram=} [properties] Properties to set
                 */
                function Histogram(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Histogram value.
                 * @member {string} value
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 */
                Histogram.prototype.value = "";

                /**
                 * Histogram maxbins.
                 * @member {number|Long} maxbins
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 */
                Histogram.prototype.maxbins = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

                /**
                 * Histogram minstep.
                 * @member {number} minstep
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 */
                Histogram.prototype.minstep = 0;

                /**
                 * Histogram horizontal.
                 * @member {boolean} horizontal
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 */
                Histogram.prototype.horizontal = false;

                /**
                 * Histogram prebinCount.
                 * @member {string} prebinCount
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 */
                Histogram.prototype.prebinCount = "";

                /**
                 * Creates a new Histogram instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {px.vispb.HistogramChart.IHistogram=} [properties] Properties to set
                 * @returns {px.vispb.HistogramChart.Histogram} Histogram instance
                 */
                Histogram.create = function create(properties) {
                    return new Histogram(properties);
                };

                /**
                 * Encodes the specified Histogram message. Does not implicitly {@link px.vispb.HistogramChart.Histogram.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {px.vispb.HistogramChart.IHistogram} message Histogram message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Histogram.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                    if (message.maxbins != null && Object.hasOwnProperty.call(message, "maxbins"))
                        writer.uint32(/* id 2, wireType 0 =*/16).int64(message.maxbins);
                    if (message.minstep != null && Object.hasOwnProperty.call(message, "minstep"))
                        writer.uint32(/* id 3, wireType 1 =*/25).double(message.minstep);
                    if (message.horizontal != null && Object.hasOwnProperty.call(message, "horizontal"))
                        writer.uint32(/* id 4, wireType 0 =*/32).bool(message.horizontal);
                    if (message.prebinCount != null && Object.hasOwnProperty.call(message, "prebinCount"))
                        writer.uint32(/* id 5, wireType 2 =*/42).string(message.prebinCount);
                    return writer;
                };

                /**
                 * Encodes the specified Histogram message, length delimited. Does not implicitly {@link px.vispb.HistogramChart.Histogram.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {px.vispb.HistogramChart.IHistogram} message Histogram message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Histogram.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Histogram message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.HistogramChart.Histogram} Histogram
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Histogram.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.HistogramChart.Histogram();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.value = reader.string();
                            break;
                        case 2:
                            message.maxbins = reader.int64();
                            break;
                        case 3:
                            message.minstep = reader.double();
                            break;
                        case 4:
                            message.horizontal = reader.bool();
                            break;
                        case 5:
                            message.prebinCount = reader.string();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Histogram message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.HistogramChart.Histogram} Histogram
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Histogram.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Histogram message.
                 * @function verify
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Histogram.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.value != null && message.hasOwnProperty("value"))
                        if (!$util.isString(message.value))
                            return "value: string expected";
                    if (message.maxbins != null && message.hasOwnProperty("maxbins"))
                        if (!$util.isInteger(message.maxbins) && !(message.maxbins && $util.isInteger(message.maxbins.low) && $util.isInteger(message.maxbins.high)))
                            return "maxbins: integer|Long expected";
                    if (message.minstep != null && message.hasOwnProperty("minstep"))
                        if (typeof message.minstep !== "number")
                            return "minstep: number expected";
                    if (message.horizontal != null && message.hasOwnProperty("horizontal"))
                        if (typeof message.horizontal !== "boolean")
                            return "horizontal: boolean expected";
                    if (message.prebinCount != null && message.hasOwnProperty("prebinCount"))
                        if (!$util.isString(message.prebinCount))
                            return "prebinCount: string expected";
                    return null;
                };

                /**
                 * Creates a Histogram message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.HistogramChart.Histogram} Histogram
                 */
                Histogram.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.HistogramChart.Histogram)
                        return object;
                    let message = new $root.px.vispb.HistogramChart.Histogram();
                    if (object.value != null)
                        message.value = String(object.value);
                    if (object.maxbins != null)
                        if ($util.Long)
                            (message.maxbins = $util.Long.fromValue(object.maxbins)).unsigned = false;
                        else if (typeof object.maxbins === "string")
                            message.maxbins = parseInt(object.maxbins, 10);
                        else if (typeof object.maxbins === "number")
                            message.maxbins = object.maxbins;
                        else if (typeof object.maxbins === "object")
                            message.maxbins = new $util.LongBits(object.maxbins.low >>> 0, object.maxbins.high >>> 0).toNumber();
                    if (object.minstep != null)
                        message.minstep = Number(object.minstep);
                    if (object.horizontal != null)
                        message.horizontal = Boolean(object.horizontal);
                    if (object.prebinCount != null)
                        message.prebinCount = String(object.prebinCount);
                    return message;
                };

                /**
                 * Creates a plain object from a Histogram message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @static
                 * @param {px.vispb.HistogramChart.Histogram} message Histogram
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Histogram.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.value = "";
                        if ($util.Long) {
                            let long = new $util.Long(0, 0, false);
                            object.maxbins = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                        } else
                            object.maxbins = options.longs === String ? "0" : 0;
                        object.minstep = 0;
                        object.horizontal = false;
                        object.prebinCount = "";
                    }
                    if (message.value != null && message.hasOwnProperty("value"))
                        object.value = message.value;
                    if (message.maxbins != null && message.hasOwnProperty("maxbins"))
                        if (typeof message.maxbins === "number")
                            object.maxbins = options.longs === String ? String(message.maxbins) : message.maxbins;
                        else
                            object.maxbins = options.longs === String ? $util.Long.prototype.toString.call(message.maxbins) : options.longs === Number ? new $util.LongBits(message.maxbins.low >>> 0, message.maxbins.high >>> 0).toNumber() : message.maxbins;
                    if (message.minstep != null && message.hasOwnProperty("minstep"))
                        object.minstep = options.json && !isFinite(message.minstep) ? String(message.minstep) : message.minstep;
                    if (message.horizontal != null && message.hasOwnProperty("horizontal"))
                        object.horizontal = message.horizontal;
                    if (message.prebinCount != null && message.hasOwnProperty("prebinCount"))
                        object.prebinCount = message.prebinCount;
                    return object;
                };

                /**
                 * Converts this Histogram to JSON.
                 * @function toJSON
                 * @memberof px.vispb.HistogramChart.Histogram
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Histogram.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return Histogram;
            })();

            return HistogramChart;
        })();

        vispb.GaugeChart = (function() {

            /**
             * Properties of a GaugeChart.
             * @memberof px.vispb
             * @interface IGaugeChart
             * @property {string|null} [value] GaugeChart value
             * @property {string|null} [title] GaugeChart title
             */

            /**
             * Constructs a new GaugeChart.
             * @memberof px.vispb
             * @classdesc Represents a GaugeChart.
             * @implements IGaugeChart
             * @constructor
             * @param {px.vispb.IGaugeChart=} [properties] Properties to set
             */
            function GaugeChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * GaugeChart value.
             * @member {string} value
             * @memberof px.vispb.GaugeChart
             * @instance
             */
            GaugeChart.prototype.value = "";

            /**
             * GaugeChart title.
             * @member {string} title
             * @memberof px.vispb.GaugeChart
             * @instance
             */
            GaugeChart.prototype.title = "";

            /**
             * Creates a new GaugeChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {px.vispb.IGaugeChart=} [properties] Properties to set
             * @returns {px.vispb.GaugeChart} GaugeChart instance
             */
            GaugeChart.create = function create(properties) {
                return new GaugeChart(properties);
            };

            /**
             * Encodes the specified GaugeChart message. Does not implicitly {@link px.vispb.GaugeChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {px.vispb.IGaugeChart} message GaugeChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            GaugeChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                return writer;
            };

            /**
             * Encodes the specified GaugeChart message, length delimited. Does not implicitly {@link px.vispb.GaugeChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {px.vispb.IGaugeChart} message GaugeChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            GaugeChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a GaugeChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.GaugeChart} GaugeChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            GaugeChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.GaugeChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.string();
                        break;
                    case 2:
                        message.title = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a GaugeChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.GaugeChart} GaugeChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            GaugeChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a GaugeChart message.
             * @function verify
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            GaugeChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isString(message.value))
                        return "value: string expected";
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                return null;
            };

            /**
             * Creates a GaugeChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.GaugeChart} GaugeChart
             */
            GaugeChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.GaugeChart)
                    return object;
                let message = new $root.px.vispb.GaugeChart();
                if (object.value != null)
                    message.value = String(object.value);
                if (object.title != null)
                    message.title = String(object.title);
                return message;
            };

            /**
             * Creates a plain object from a GaugeChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.GaugeChart
             * @static
             * @param {px.vispb.GaugeChart} message GaugeChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            GaugeChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.value = "";
                    object.title = "";
                }
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                return object;
            };

            /**
             * Converts this GaugeChart to JSON.
             * @function toJSON
             * @memberof px.vispb.GaugeChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            GaugeChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return GaugeChart;
        })();

        vispb.TimeseriesChart = (function() {

            /**
             * Properties of a TimeseriesChart.
             * @memberof px.vispb
             * @interface ITimeseriesChart
             * @property {Array.<px.vispb.TimeseriesChart.ITimeseries>|null} [timeseries] TimeseriesChart timeseries
             * @property {string|null} [title] TimeseriesChart title
             * @property {px.vispb.IAxis|null} [xAxis] TimeseriesChart xAxis
             * @property {px.vispb.IAxis|null} [yAxis] TimeseriesChart yAxis
             */

            /**
             * Constructs a new TimeseriesChart.
             * @memberof px.vispb
             * @classdesc Represents a TimeseriesChart.
             * @implements ITimeseriesChart
             * @constructor
             * @param {px.vispb.ITimeseriesChart=} [properties] Properties to set
             */
            function TimeseriesChart(properties) {
                this.timeseries = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * TimeseriesChart timeseries.
             * @member {Array.<px.vispb.TimeseriesChart.ITimeseries>} timeseries
             * @memberof px.vispb.TimeseriesChart
             * @instance
             */
            TimeseriesChart.prototype.timeseries = $util.emptyArray;

            /**
             * TimeseriesChart title.
             * @member {string} title
             * @memberof px.vispb.TimeseriesChart
             * @instance
             */
            TimeseriesChart.prototype.title = "";

            /**
             * TimeseriesChart xAxis.
             * @member {px.vispb.IAxis|null|undefined} xAxis
             * @memberof px.vispb.TimeseriesChart
             * @instance
             */
            TimeseriesChart.prototype.xAxis = null;

            /**
             * TimeseriesChart yAxis.
             * @member {px.vispb.IAxis|null|undefined} yAxis
             * @memberof px.vispb.TimeseriesChart
             * @instance
             */
            TimeseriesChart.prototype.yAxis = null;

            /**
             * Creates a new TimeseriesChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {px.vispb.ITimeseriesChart=} [properties] Properties to set
             * @returns {px.vispb.TimeseriesChart} TimeseriesChart instance
             */
            TimeseriesChart.create = function create(properties) {
                return new TimeseriesChart(properties);
            };

            /**
             * Encodes the specified TimeseriesChart message. Does not implicitly {@link px.vispb.TimeseriesChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {px.vispb.ITimeseriesChart} message TimeseriesChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            TimeseriesChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.timeseries != null && message.timeseries.length)
                    for (let i = 0; i < message.timeseries.length; ++i)
                        $root.px.vispb.TimeseriesChart.Timeseries.encode(message.timeseries[i], writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                if (message.xAxis != null && Object.hasOwnProperty.call(message, "xAxis"))
                    $root.px.vispb.Axis.encode(message.xAxis, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
                if (message.yAxis != null && Object.hasOwnProperty.call(message, "yAxis"))
                    $root.px.vispb.Axis.encode(message.yAxis, writer.uint32(/* id 4, wireType 2 =*/34).fork()).ldelim();
                return writer;
            };

            /**
             * Encodes the specified TimeseriesChart message, length delimited. Does not implicitly {@link px.vispb.TimeseriesChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {px.vispb.ITimeseriesChart} message TimeseriesChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            TimeseriesChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a TimeseriesChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.TimeseriesChart} TimeseriesChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            TimeseriesChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.TimeseriesChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        if (!(message.timeseries && message.timeseries.length))
                            message.timeseries = [];
                        message.timeseries.push($root.px.vispb.TimeseriesChart.Timeseries.decode(reader, reader.uint32()));
                        break;
                    case 2:
                        message.title = reader.string();
                        break;
                    case 3:
                        message.xAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    case 4:
                        message.yAxis = $root.px.vispb.Axis.decode(reader, reader.uint32());
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a TimeseriesChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.TimeseriesChart} TimeseriesChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            TimeseriesChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a TimeseriesChart message.
             * @function verify
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            TimeseriesChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.timeseries != null && message.hasOwnProperty("timeseries")) {
                    if (!Array.isArray(message.timeseries))
                        return "timeseries: array expected";
                    for (let i = 0; i < message.timeseries.length; ++i) {
                        let error = $root.px.vispb.TimeseriesChart.Timeseries.verify(message.timeseries[i]);
                        if (error)
                            return "timeseries." + error;
                    }
                }
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                if (message.xAxis != null && message.hasOwnProperty("xAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.xAxis);
                    if (error)
                        return "xAxis." + error;
                }
                if (message.yAxis != null && message.hasOwnProperty("yAxis")) {
                    let error = $root.px.vispb.Axis.verify(message.yAxis);
                    if (error)
                        return "yAxis." + error;
                }
                return null;
            };

            /**
             * Creates a TimeseriesChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.TimeseriesChart} TimeseriesChart
             */
            TimeseriesChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.TimeseriesChart)
                    return object;
                let message = new $root.px.vispb.TimeseriesChart();
                if (object.timeseries) {
                    if (!Array.isArray(object.timeseries))
                        throw TypeError(".px.vispb.TimeseriesChart.timeseries: array expected");
                    message.timeseries = [];
                    for (let i = 0; i < object.timeseries.length; ++i) {
                        if (typeof object.timeseries[i] !== "object")
                            throw TypeError(".px.vispb.TimeseriesChart.timeseries: object expected");
                        message.timeseries[i] = $root.px.vispb.TimeseriesChart.Timeseries.fromObject(object.timeseries[i]);
                    }
                }
                if (object.title != null)
                    message.title = String(object.title);
                if (object.xAxis != null) {
                    if (typeof object.xAxis !== "object")
                        throw TypeError(".px.vispb.TimeseriesChart.xAxis: object expected");
                    message.xAxis = $root.px.vispb.Axis.fromObject(object.xAxis);
                }
                if (object.yAxis != null) {
                    if (typeof object.yAxis !== "object")
                        throw TypeError(".px.vispb.TimeseriesChart.yAxis: object expected");
                    message.yAxis = $root.px.vispb.Axis.fromObject(object.yAxis);
                }
                return message;
            };

            /**
             * Creates a plain object from a TimeseriesChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.TimeseriesChart
             * @static
             * @param {px.vispb.TimeseriesChart} message TimeseriesChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            TimeseriesChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults)
                    object.timeseries = [];
                if (options.defaults) {
                    object.title = "";
                    object.xAxis = null;
                    object.yAxis = null;
                }
                if (message.timeseries && message.timeseries.length) {
                    object.timeseries = [];
                    for (let j = 0; j < message.timeseries.length; ++j)
                        object.timeseries[j] = $root.px.vispb.TimeseriesChart.Timeseries.toObject(message.timeseries[j], options);
                }
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                if (message.xAxis != null && message.hasOwnProperty("xAxis"))
                    object.xAxis = $root.px.vispb.Axis.toObject(message.xAxis, options);
                if (message.yAxis != null && message.hasOwnProperty("yAxis"))
                    object.yAxis = $root.px.vispb.Axis.toObject(message.yAxis, options);
                return object;
            };

            /**
             * Converts this TimeseriesChart to JSON.
             * @function toJSON
             * @memberof px.vispb.TimeseriesChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            TimeseriesChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            TimeseriesChart.Timeseries = (function() {

                /**
                 * Properties of a Timeseries.
                 * @memberof px.vispb.TimeseriesChart
                 * @interface ITimeseries
                 * @property {string|null} [value] Timeseries value
                 * @property {string|null} [series] Timeseries series
                 * @property {boolean|null} [stackBySeries] Timeseries stackBySeries
                 * @property {px.vispb.TimeseriesChart.Timeseries.Mode|null} [mode] Timeseries mode
                 */

                /**
                 * Constructs a new Timeseries.
                 * @memberof px.vispb.TimeseriesChart
                 * @classdesc Represents a Timeseries.
                 * @implements ITimeseries
                 * @constructor
                 * @param {px.vispb.TimeseriesChart.ITimeseries=} [properties] Properties to set
                 */
                function Timeseries(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Timeseries value.
                 * @member {string} value
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @instance
                 */
                Timeseries.prototype.value = "";

                /**
                 * Timeseries series.
                 * @member {string} series
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @instance
                 */
                Timeseries.prototype.series = "";

                /**
                 * Timeseries stackBySeries.
                 * @member {boolean} stackBySeries
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @instance
                 */
                Timeseries.prototype.stackBySeries = false;

                /**
                 * Timeseries mode.
                 * @member {px.vispb.TimeseriesChart.Timeseries.Mode} mode
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @instance
                 */
                Timeseries.prototype.mode = 0;

                /**
                 * Creates a new Timeseries instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {px.vispb.TimeseriesChart.ITimeseries=} [properties] Properties to set
                 * @returns {px.vispb.TimeseriesChart.Timeseries} Timeseries instance
                 */
                Timeseries.create = function create(properties) {
                    return new Timeseries(properties);
                };

                /**
                 * Encodes the specified Timeseries message. Does not implicitly {@link px.vispb.TimeseriesChart.Timeseries.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {px.vispb.TimeseriesChart.ITimeseries} message Timeseries message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Timeseries.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                    if (message.series != null && Object.hasOwnProperty.call(message, "series"))
                        writer.uint32(/* id 2, wireType 2 =*/18).string(message.series);
                    if (message.stackBySeries != null && Object.hasOwnProperty.call(message, "stackBySeries"))
                        writer.uint32(/* id 3, wireType 0 =*/24).bool(message.stackBySeries);
                    if (message.mode != null && Object.hasOwnProperty.call(message, "mode"))
                        writer.uint32(/* id 4, wireType 0 =*/32).int32(message.mode);
                    return writer;
                };

                /**
                 * Encodes the specified Timeseries message, length delimited. Does not implicitly {@link px.vispb.TimeseriesChart.Timeseries.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {px.vispb.TimeseriesChart.ITimeseries} message Timeseries message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Timeseries.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Timeseries message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.TimeseriesChart.Timeseries} Timeseries
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Timeseries.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.TimeseriesChart.Timeseries();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.value = reader.string();
                            break;
                        case 2:
                            message.series = reader.string();
                            break;
                        case 3:
                            message.stackBySeries = reader.bool();
                            break;
                        case 4:
                            message.mode = reader.int32();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Timeseries message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.TimeseriesChart.Timeseries} Timeseries
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Timeseries.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Timeseries message.
                 * @function verify
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Timeseries.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.value != null && message.hasOwnProperty("value"))
                        if (!$util.isString(message.value))
                            return "value: string expected";
                    if (message.series != null && message.hasOwnProperty("series"))
                        if (!$util.isString(message.series))
                            return "series: string expected";
                    if (message.stackBySeries != null && message.hasOwnProperty("stackBySeries"))
                        if (typeof message.stackBySeries !== "boolean")
                            return "stackBySeries: boolean expected";
                    if (message.mode != null && message.hasOwnProperty("mode"))
                        switch (message.mode) {
                        default:
                            return "mode: enum value expected";
                        case 0:
                        case 2:
                        case 3:
                        case 4:
                            break;
                        }
                    return null;
                };

                /**
                 * Creates a Timeseries message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.TimeseriesChart.Timeseries} Timeseries
                 */
                Timeseries.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.TimeseriesChart.Timeseries)
                        return object;
                    let message = new $root.px.vispb.TimeseriesChart.Timeseries();
                    if (object.value != null)
                        message.value = String(object.value);
                    if (object.series != null)
                        message.series = String(object.series);
                    if (object.stackBySeries != null)
                        message.stackBySeries = Boolean(object.stackBySeries);
                    switch (object.mode) {
                    case "MODE_UNKNOWN":
                    case 0:
                        message.mode = 0;
                        break;
                    case "MODE_LINE":
                    case 2:
                        message.mode = 2;
                        break;
                    case "MODE_POINT":
                    case 3:
                        message.mode = 3;
                        break;
                    case "MODE_AREA":
                    case 4:
                        message.mode = 4;
                        break;
                    }
                    return message;
                };

                /**
                 * Creates a plain object from a Timeseries message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @static
                 * @param {px.vispb.TimeseriesChart.Timeseries} message Timeseries
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Timeseries.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.value = "";
                        object.series = "";
                        object.stackBySeries = false;
                        object.mode = options.enums === String ? "MODE_UNKNOWN" : 0;
                    }
                    if (message.value != null && message.hasOwnProperty("value"))
                        object.value = message.value;
                    if (message.series != null && message.hasOwnProperty("series"))
                        object.series = message.series;
                    if (message.stackBySeries != null && message.hasOwnProperty("stackBySeries"))
                        object.stackBySeries = message.stackBySeries;
                    if (message.mode != null && message.hasOwnProperty("mode"))
                        object.mode = options.enums === String ? $root.px.vispb.TimeseriesChart.Timeseries.Mode[message.mode] : message.mode;
                    return object;
                };

                /**
                 * Converts this Timeseries to JSON.
                 * @function toJSON
                 * @memberof px.vispb.TimeseriesChart.Timeseries
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Timeseries.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                /**
                 * Mode enum.
                 * @name px.vispb.TimeseriesChart.Timeseries.Mode
                 * @enum {number}
                 * @property {number} MODE_UNKNOWN=0 MODE_UNKNOWN value
                 * @property {number} MODE_LINE=2 MODE_LINE value
                 * @property {number} MODE_POINT=3 MODE_POINT value
                 * @property {number} MODE_AREA=4 MODE_AREA value
                 */
                Timeseries.Mode = (function() {
                    const valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "MODE_UNKNOWN"] = 0;
                    values[valuesById[2] = "MODE_LINE"] = 2;
                    values[valuesById[3] = "MODE_POINT"] = 3;
                    values[valuesById[4] = "MODE_AREA"] = 4;
                    return values;
                })();

                return Timeseries;
            })();

            return TimeseriesChart;
        })();

        vispb.StatChart = (function() {

            /**
             * Properties of a StatChart.
             * @memberof px.vispb
             * @interface IStatChart
             * @property {px.vispb.StatChart.IStat|null} [stat] StatChart stat
             * @property {string|null} [title] StatChart title
             */

            /**
             * Constructs a new StatChart.
             * @memberof px.vispb
             * @classdesc Represents a StatChart.
             * @implements IStatChart
             * @constructor
             * @param {px.vispb.IStatChart=} [properties] Properties to set
             */
            function StatChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * StatChart stat.
             * @member {px.vispb.StatChart.IStat|null|undefined} stat
             * @memberof px.vispb.StatChart
             * @instance
             */
            StatChart.prototype.stat = null;

            /**
             * StatChart title.
             * @member {string} title
             * @memberof px.vispb.StatChart
             * @instance
             */
            StatChart.prototype.title = "";

            /**
             * Creates a new StatChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.StatChart
             * @static
             * @param {px.vispb.IStatChart=} [properties] Properties to set
             * @returns {px.vispb.StatChart} StatChart instance
             */
            StatChart.create = function create(properties) {
                return new StatChart(properties);
            };

            /**
             * Encodes the specified StatChart message. Does not implicitly {@link px.vispb.StatChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.StatChart
             * @static
             * @param {px.vispb.IStatChart} message StatChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StatChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.stat != null && Object.hasOwnProperty.call(message, "stat"))
                    $root.px.vispb.StatChart.Stat.encode(message.stat, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
                if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                return writer;
            };

            /**
             * Encodes the specified StatChart message, length delimited. Does not implicitly {@link px.vispb.StatChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.StatChart
             * @static
             * @param {px.vispb.IStatChart} message StatChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StatChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a StatChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.StatChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.StatChart} StatChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StatChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.StatChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.stat = $root.px.vispb.StatChart.Stat.decode(reader, reader.uint32());
                        break;
                    case 2:
                        message.title = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a StatChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.StatChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.StatChart} StatChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StatChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a StatChart message.
             * @function verify
             * @memberof px.vispb.StatChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            StatChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.stat != null && message.hasOwnProperty("stat")) {
                    let error = $root.px.vispb.StatChart.Stat.verify(message.stat);
                    if (error)
                        return "stat." + error;
                }
                if (message.title != null && message.hasOwnProperty("title"))
                    if (!$util.isString(message.title))
                        return "title: string expected";
                return null;
            };

            /**
             * Creates a StatChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.StatChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.StatChart} StatChart
             */
            StatChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.StatChart)
                    return object;
                let message = new $root.px.vispb.StatChart();
                if (object.stat != null) {
                    if (typeof object.stat !== "object")
                        throw TypeError(".px.vispb.StatChart.stat: object expected");
                    message.stat = $root.px.vispb.StatChart.Stat.fromObject(object.stat);
                }
                if (object.title != null)
                    message.title = String(object.title);
                return message;
            };

            /**
             * Creates a plain object from a StatChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.StatChart
             * @static
             * @param {px.vispb.StatChart} message StatChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            StatChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.stat = null;
                    object.title = "";
                }
                if (message.stat != null && message.hasOwnProperty("stat"))
                    object.stat = $root.px.vispb.StatChart.Stat.toObject(message.stat, options);
                if (message.title != null && message.hasOwnProperty("title"))
                    object.title = message.title;
                return object;
            };

            /**
             * Converts this StatChart to JSON.
             * @function toJSON
             * @memberof px.vispb.StatChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            StatChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            StatChart.Stat = (function() {

                /**
                 * Properties of a Stat.
                 * @memberof px.vispb.StatChart
                 * @interface IStat
                 * @property {string|null} [value] Stat value
                 */

                /**
                 * Constructs a new Stat.
                 * @memberof px.vispb.StatChart
                 * @classdesc Represents a Stat.
                 * @implements IStat
                 * @constructor
                 * @param {px.vispb.StatChart.IStat=} [properties] Properties to set
                 */
                function Stat(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * Stat value.
                 * @member {string} value
                 * @memberof px.vispb.StatChart.Stat
                 * @instance
                 */
                Stat.prototype.value = "";

                /**
                 * Creates a new Stat instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {px.vispb.StatChart.IStat=} [properties] Properties to set
                 * @returns {px.vispb.StatChart.Stat} Stat instance
                 */
                Stat.create = function create(properties) {
                    return new Stat(properties);
                };

                /**
                 * Encodes the specified Stat message. Does not implicitly {@link px.vispb.StatChart.Stat.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {px.vispb.StatChart.IStat} message Stat message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Stat.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                    return writer;
                };

                /**
                 * Encodes the specified Stat message, length delimited. Does not implicitly {@link px.vispb.StatChart.Stat.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {px.vispb.StatChart.IStat} message Stat message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                Stat.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes a Stat message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.StatChart.Stat} Stat
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Stat.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.StatChart.Stat();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.value = reader.string();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes a Stat message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.StatChart.Stat} Stat
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                Stat.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies a Stat message.
                 * @function verify
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                Stat.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.value != null && message.hasOwnProperty("value"))
                        if (!$util.isString(message.value))
                            return "value: string expected";
                    return null;
                };

                /**
                 * Creates a Stat message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.StatChart.Stat} Stat
                 */
                Stat.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.StatChart.Stat)
                        return object;
                    let message = new $root.px.vispb.StatChart.Stat();
                    if (object.value != null)
                        message.value = String(object.value);
                    return message;
                };

                /**
                 * Creates a plain object from a Stat message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.StatChart.Stat
                 * @static
                 * @param {px.vispb.StatChart.Stat} message Stat
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                Stat.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults)
                        object.value = "";
                    if (message.value != null && message.hasOwnProperty("value"))
                        object.value = message.value;
                    return object;
                };

                /**
                 * Converts this Stat to JSON.
                 * @function toJSON
                 * @memberof px.vispb.StatChart.Stat
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                Stat.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return Stat;
            })();

            return StatChart;
        })();

        vispb.TextChart = (function() {

            /**
             * Properties of a TextChart.
             * @memberof px.vispb
             * @interface ITextChart
             * @property {string|null} [body] TextChart body
             */

            /**
             * Constructs a new TextChart.
             * @memberof px.vispb
             * @classdesc Represents a TextChart.
             * @implements ITextChart
             * @constructor
             * @param {px.vispb.ITextChart=} [properties] Properties to set
             */
            function TextChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * TextChart body.
             * @member {string} body
             * @memberof px.vispb.TextChart
             * @instance
             */
            TextChart.prototype.body = "";

            /**
             * Creates a new TextChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.TextChart
             * @static
             * @param {px.vispb.ITextChart=} [properties] Properties to set
             * @returns {px.vispb.TextChart} TextChart instance
             */
            TextChart.create = function create(properties) {
                return new TextChart(properties);
            };

            /**
             * Encodes the specified TextChart message. Does not implicitly {@link px.vispb.TextChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.TextChart
             * @static
             * @param {px.vispb.ITextChart} message TextChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            TextChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.body != null && Object.hasOwnProperty.call(message, "body"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.body);
                return writer;
            };

            /**
             * Encodes the specified TextChart message, length delimited. Does not implicitly {@link px.vispb.TextChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.TextChart
             * @static
             * @param {px.vispb.ITextChart} message TextChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            TextChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a TextChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.TextChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.TextChart} TextChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            TextChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.TextChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.body = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a TextChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.TextChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.TextChart} TextChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            TextChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a TextChart message.
             * @function verify
             * @memberof px.vispb.TextChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            TextChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.body != null && message.hasOwnProperty("body"))
                    if (!$util.isString(message.body))
                        return "body: string expected";
                return null;
            };

            /**
             * Creates a TextChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.TextChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.TextChart} TextChart
             */
            TextChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.TextChart)
                    return object;
                let message = new $root.px.vispb.TextChart();
                if (object.body != null)
                    message.body = String(object.body);
                return message;
            };

            /**
             * Creates a plain object from a TextChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.TextChart
             * @static
             * @param {px.vispb.TextChart} message TextChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            TextChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.body = "";
                if (message.body != null && message.hasOwnProperty("body"))
                    object.body = message.body;
                return object;
            };

            /**
             * Converts this TextChart to JSON.
             * @function toJSON
             * @memberof px.vispb.TextChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            TextChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return TextChart;
        })();

        vispb.VegaChart = (function() {

            /**
             * Properties of a VegaChart.
             * @memberof px.vispb
             * @interface IVegaChart
             * @property {string|null} [spec] VegaChart spec
             */

            /**
             * Constructs a new VegaChart.
             * @memberof px.vispb
             * @classdesc Represents a VegaChart.
             * @implements IVegaChart
             * @constructor
             * @param {px.vispb.IVegaChart=} [properties] Properties to set
             */
            function VegaChart(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * VegaChart spec.
             * @member {string} spec
             * @memberof px.vispb.VegaChart
             * @instance
             */
            VegaChart.prototype.spec = "";

            /**
             * Creates a new VegaChart instance using the specified properties.
             * @function create
             * @memberof px.vispb.VegaChart
             * @static
             * @param {px.vispb.IVegaChart=} [properties] Properties to set
             * @returns {px.vispb.VegaChart} VegaChart instance
             */
            VegaChart.create = function create(properties) {
                return new VegaChart(properties);
            };

            /**
             * Encodes the specified VegaChart message. Does not implicitly {@link px.vispb.VegaChart.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.VegaChart
             * @static
             * @param {px.vispb.IVegaChart} message VegaChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            VegaChart.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.spec != null && Object.hasOwnProperty.call(message, "spec"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.spec);
                return writer;
            };

            /**
             * Encodes the specified VegaChart message, length delimited. Does not implicitly {@link px.vispb.VegaChart.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.VegaChart
             * @static
             * @param {px.vispb.IVegaChart} message VegaChart message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            VegaChart.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a VegaChart message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.VegaChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.VegaChart} VegaChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            VegaChart.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.VegaChart();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.spec = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a VegaChart message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.VegaChart
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.VegaChart} VegaChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            VegaChart.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a VegaChart message.
             * @function verify
             * @memberof px.vispb.VegaChart
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            VegaChart.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.spec != null && message.hasOwnProperty("spec"))
                    if (!$util.isString(message.spec))
                        return "spec: string expected";
                return null;
            };

            /**
             * Creates a VegaChart message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.VegaChart
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.VegaChart} VegaChart
             */
            VegaChart.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.VegaChart)
                    return object;
                let message = new $root.px.vispb.VegaChart();
                if (object.spec != null)
                    message.spec = String(object.spec);
                return message;
            };

            /**
             * Creates a plain object from a VegaChart message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.VegaChart
             * @static
             * @param {px.vispb.VegaChart} message VegaChart
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            VegaChart.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.spec = "";
                if (message.spec != null && message.hasOwnProperty("spec"))
                    object.spec = message.spec;
                return object;
            };

            /**
             * Converts this VegaChart to JSON.
             * @function toJSON
             * @memberof px.vispb.VegaChart
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            VegaChart.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return VegaChart;
        })();

        vispb.Table = (function() {

            /**
             * Properties of a Table.
             * @memberof px.vispb
             * @interface ITable
             * @property {string|null} [gutterColumn] Table gutterColumn
             */

            /**
             * Constructs a new Table.
             * @memberof px.vispb
             * @classdesc Represents a Table.
             * @implements ITable
             * @constructor
             * @param {px.vispb.ITable=} [properties] Properties to set
             */
            function Table(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Table gutterColumn.
             * @member {string} gutterColumn
             * @memberof px.vispb.Table
             * @instance
             */
            Table.prototype.gutterColumn = "";

            /**
             * Creates a new Table instance using the specified properties.
             * @function create
             * @memberof px.vispb.Table
             * @static
             * @param {px.vispb.ITable=} [properties] Properties to set
             * @returns {px.vispb.Table} Table instance
             */
            Table.create = function create(properties) {
                return new Table(properties);
            };

            /**
             * Encodes the specified Table message. Does not implicitly {@link px.vispb.Table.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.Table
             * @static
             * @param {px.vispb.ITable} message Table message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Table.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.gutterColumn != null && Object.hasOwnProperty.call(message, "gutterColumn"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.gutterColumn);
                return writer;
            };

            /**
             * Encodes the specified Table message, length delimited. Does not implicitly {@link px.vispb.Table.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.Table
             * @static
             * @param {px.vispb.ITable} message Table message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Table.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a Table message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.Table
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.Table} Table
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Table.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Table();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.gutterColumn = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a Table message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.Table
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.Table} Table
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Table.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a Table message.
             * @function verify
             * @memberof px.vispb.Table
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Table.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.gutterColumn != null && message.hasOwnProperty("gutterColumn"))
                    if (!$util.isString(message.gutterColumn))
                        return "gutterColumn: string expected";
                return null;
            };

            /**
             * Creates a Table message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.Table
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.Table} Table
             */
            Table.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.Table)
                    return object;
                let message = new $root.px.vispb.Table();
                if (object.gutterColumn != null)
                    message.gutterColumn = String(object.gutterColumn);
                return message;
            };

            /**
             * Creates a plain object from a Table message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.Table
             * @static
             * @param {px.vispb.Table} message Table
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Table.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.gutterColumn = "";
                if (message.gutterColumn != null && message.hasOwnProperty("gutterColumn"))
                    object.gutterColumn = message.gutterColumn;
                return object;
            };

            /**
             * Converts this Table to JSON.
             * @function toJSON
             * @memberof px.vispb.Table
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Table.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return Table;
        })();

        vispb.Graph = (function() {

            /**
             * Properties of a Graph.
             * @memberof px.vispb
             * @interface IGraph
             * @property {string|null} [dotColumn] Graph dotColumn
             * @property {px.vispb.Graph.IAdjacencyList|null} [adjacencyList] Graph adjacencyList
             * @property {string|null} [edgeWeightColumn] Graph edgeWeightColumn
             * @property {string|null} [nodeWeightColumn] Graph nodeWeightColumn
             * @property {string|null} [edgeColorColumn] Graph edgeColorColumn
             * @property {px.vispb.Graph.IEdgeThresholds|null} [edgeThresholds] Graph edgeThresholds
             * @property {Array.<string>|null} [edgeHoverInfo] Graph edgeHoverInfo
             * @property {number|Long|null} [edgeLength] Graph edgeLength
             * @property {boolean|null} [enableDefaultHierarchy] Graph enableDefaultHierarchy
             */

            /**
             * Constructs a new Graph.
             * @memberof px.vispb
             * @classdesc Represents a Graph.
             * @implements IGraph
             * @constructor
             * @param {px.vispb.IGraph=} [properties] Properties to set
             */
            function Graph(properties) {
                this.edgeHoverInfo = [];
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Graph dotColumn.
             * @member {string|null|undefined} dotColumn
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.dotColumn = null;

            /**
             * Graph adjacencyList.
             * @member {px.vispb.Graph.IAdjacencyList|null|undefined} adjacencyList
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.adjacencyList = null;

            /**
             * Graph edgeWeightColumn.
             * @member {string} edgeWeightColumn
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.edgeWeightColumn = "";

            /**
             * Graph nodeWeightColumn.
             * @member {string} nodeWeightColumn
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.nodeWeightColumn = "";

            /**
             * Graph edgeColorColumn.
             * @member {string} edgeColorColumn
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.edgeColorColumn = "";

            /**
             * Graph edgeThresholds.
             * @member {px.vispb.Graph.IEdgeThresholds|null|undefined} edgeThresholds
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.edgeThresholds = null;

            /**
             * Graph edgeHoverInfo.
             * @member {Array.<string>} edgeHoverInfo
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.edgeHoverInfo = $util.emptyArray;

            /**
             * Graph edgeLength.
             * @member {number|Long} edgeLength
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.edgeLength = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

            /**
             * Graph enableDefaultHierarchy.
             * @member {boolean} enableDefaultHierarchy
             * @memberof px.vispb.Graph
             * @instance
             */
            Graph.prototype.enableDefaultHierarchy = false;

            // OneOf field names bound to virtual getters and setters
            let $oneOfFields;

            /**
             * Graph input.
             * @member {"dotColumn"|"adjacencyList"|undefined} input
             * @memberof px.vispb.Graph
             * @instance
             */
            Object.defineProperty(Graph.prototype, "input", {
                get: $util.oneOfGetter($oneOfFields = ["dotColumn", "adjacencyList"]),
                set: $util.oneOfSetter($oneOfFields)
            });

            /**
             * Creates a new Graph instance using the specified properties.
             * @function create
             * @memberof px.vispb.Graph
             * @static
             * @param {px.vispb.IGraph=} [properties] Properties to set
             * @returns {px.vispb.Graph} Graph instance
             */
            Graph.create = function create(properties) {
                return new Graph(properties);
            };

            /**
             * Encodes the specified Graph message. Does not implicitly {@link px.vispb.Graph.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.Graph
             * @static
             * @param {px.vispb.IGraph} message Graph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Graph.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.dotColumn != null && Object.hasOwnProperty.call(message, "dotColumn"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.dotColumn);
                if (message.adjacencyList != null && Object.hasOwnProperty.call(message, "adjacencyList"))
                    $root.px.vispb.Graph.AdjacencyList.encode(message.adjacencyList, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
                if (message.edgeWeightColumn != null && Object.hasOwnProperty.call(message, "edgeWeightColumn"))
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.edgeWeightColumn);
                if (message.nodeWeightColumn != null && Object.hasOwnProperty.call(message, "nodeWeightColumn"))
                    writer.uint32(/* id 4, wireType 2 =*/34).string(message.nodeWeightColumn);
                if (message.edgeColorColumn != null && Object.hasOwnProperty.call(message, "edgeColorColumn"))
                    writer.uint32(/* id 5, wireType 2 =*/42).string(message.edgeColorColumn);
                if (message.edgeThresholds != null && Object.hasOwnProperty.call(message, "edgeThresholds"))
                    $root.px.vispb.Graph.EdgeThresholds.encode(message.edgeThresholds, writer.uint32(/* id 6, wireType 2 =*/50).fork()).ldelim();
                if (message.edgeHoverInfo != null && message.edgeHoverInfo.length)
                    for (let i = 0; i < message.edgeHoverInfo.length; ++i)
                        writer.uint32(/* id 7, wireType 2 =*/58).string(message.edgeHoverInfo[i]);
                if (message.edgeLength != null && Object.hasOwnProperty.call(message, "edgeLength"))
                    writer.uint32(/* id 8, wireType 0 =*/64).int64(message.edgeLength);
                if (message.enableDefaultHierarchy != null && Object.hasOwnProperty.call(message, "enableDefaultHierarchy"))
                    writer.uint32(/* id 9, wireType 0 =*/72).bool(message.enableDefaultHierarchy);
                return writer;
            };

            /**
             * Encodes the specified Graph message, length delimited. Does not implicitly {@link px.vispb.Graph.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.Graph
             * @static
             * @param {px.vispb.IGraph} message Graph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Graph.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a Graph message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.Graph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.Graph} Graph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Graph.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Graph();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.dotColumn = reader.string();
                        break;
                    case 2:
                        message.adjacencyList = $root.px.vispb.Graph.AdjacencyList.decode(reader, reader.uint32());
                        break;
                    case 3:
                        message.edgeWeightColumn = reader.string();
                        break;
                    case 4:
                        message.nodeWeightColumn = reader.string();
                        break;
                    case 5:
                        message.edgeColorColumn = reader.string();
                        break;
                    case 6:
                        message.edgeThresholds = $root.px.vispb.Graph.EdgeThresholds.decode(reader, reader.uint32());
                        break;
                    case 7:
                        if (!(message.edgeHoverInfo && message.edgeHoverInfo.length))
                            message.edgeHoverInfo = [];
                        message.edgeHoverInfo.push(reader.string());
                        break;
                    case 8:
                        message.edgeLength = reader.int64();
                        break;
                    case 9:
                        message.enableDefaultHierarchy = reader.bool();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a Graph message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.Graph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.Graph} Graph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Graph.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a Graph message.
             * @function verify
             * @memberof px.vispb.Graph
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Graph.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                let properties = {};
                if (message.dotColumn != null && message.hasOwnProperty("dotColumn")) {
                    properties.input = 1;
                    if (!$util.isString(message.dotColumn))
                        return "dotColumn: string expected";
                }
                if (message.adjacencyList != null && message.hasOwnProperty("adjacencyList")) {
                    if (properties.input === 1)
                        return "input: multiple values";
                    properties.input = 1;
                    {
                        let error = $root.px.vispb.Graph.AdjacencyList.verify(message.adjacencyList);
                        if (error)
                            return "adjacencyList." + error;
                    }
                }
                if (message.edgeWeightColumn != null && message.hasOwnProperty("edgeWeightColumn"))
                    if (!$util.isString(message.edgeWeightColumn))
                        return "edgeWeightColumn: string expected";
                if (message.nodeWeightColumn != null && message.hasOwnProperty("nodeWeightColumn"))
                    if (!$util.isString(message.nodeWeightColumn))
                        return "nodeWeightColumn: string expected";
                if (message.edgeColorColumn != null && message.hasOwnProperty("edgeColorColumn"))
                    if (!$util.isString(message.edgeColorColumn))
                        return "edgeColorColumn: string expected";
                if (message.edgeThresholds != null && message.hasOwnProperty("edgeThresholds")) {
                    let error = $root.px.vispb.Graph.EdgeThresholds.verify(message.edgeThresholds);
                    if (error)
                        return "edgeThresholds." + error;
                }
                if (message.edgeHoverInfo != null && message.hasOwnProperty("edgeHoverInfo")) {
                    if (!Array.isArray(message.edgeHoverInfo))
                        return "edgeHoverInfo: array expected";
                    for (let i = 0; i < message.edgeHoverInfo.length; ++i)
                        if (!$util.isString(message.edgeHoverInfo[i]))
                            return "edgeHoverInfo: string[] expected";
                }
                if (message.edgeLength != null && message.hasOwnProperty("edgeLength"))
                    if (!$util.isInteger(message.edgeLength) && !(message.edgeLength && $util.isInteger(message.edgeLength.low) && $util.isInteger(message.edgeLength.high)))
                        return "edgeLength: integer|Long expected";
                if (message.enableDefaultHierarchy != null && message.hasOwnProperty("enableDefaultHierarchy"))
                    if (typeof message.enableDefaultHierarchy !== "boolean")
                        return "enableDefaultHierarchy: boolean expected";
                return null;
            };

            /**
             * Creates a Graph message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.Graph
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.Graph} Graph
             */
            Graph.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.Graph)
                    return object;
                let message = new $root.px.vispb.Graph();
                if (object.dotColumn != null)
                    message.dotColumn = String(object.dotColumn);
                if (object.adjacencyList != null) {
                    if (typeof object.adjacencyList !== "object")
                        throw TypeError(".px.vispb.Graph.adjacencyList: object expected");
                    message.adjacencyList = $root.px.vispb.Graph.AdjacencyList.fromObject(object.adjacencyList);
                }
                if (object.edgeWeightColumn != null)
                    message.edgeWeightColumn = String(object.edgeWeightColumn);
                if (object.nodeWeightColumn != null)
                    message.nodeWeightColumn = String(object.nodeWeightColumn);
                if (object.edgeColorColumn != null)
                    message.edgeColorColumn = String(object.edgeColorColumn);
                if (object.edgeThresholds != null) {
                    if (typeof object.edgeThresholds !== "object")
                        throw TypeError(".px.vispb.Graph.edgeThresholds: object expected");
                    message.edgeThresholds = $root.px.vispb.Graph.EdgeThresholds.fromObject(object.edgeThresholds);
                }
                if (object.edgeHoverInfo) {
                    if (!Array.isArray(object.edgeHoverInfo))
                        throw TypeError(".px.vispb.Graph.edgeHoverInfo: array expected");
                    message.edgeHoverInfo = [];
                    for (let i = 0; i < object.edgeHoverInfo.length; ++i)
                        message.edgeHoverInfo[i] = String(object.edgeHoverInfo[i]);
                }
                if (object.edgeLength != null)
                    if ($util.Long)
                        (message.edgeLength = $util.Long.fromValue(object.edgeLength)).unsigned = false;
                    else if (typeof object.edgeLength === "string")
                        message.edgeLength = parseInt(object.edgeLength, 10);
                    else if (typeof object.edgeLength === "number")
                        message.edgeLength = object.edgeLength;
                    else if (typeof object.edgeLength === "object")
                        message.edgeLength = new $util.LongBits(object.edgeLength.low >>> 0, object.edgeLength.high >>> 0).toNumber();
                if (object.enableDefaultHierarchy != null)
                    message.enableDefaultHierarchy = Boolean(object.enableDefaultHierarchy);
                return message;
            };

            /**
             * Creates a plain object from a Graph message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.Graph
             * @static
             * @param {px.vispb.Graph} message Graph
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Graph.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.arrays || options.defaults)
                    object.edgeHoverInfo = [];
                if (options.defaults) {
                    object.edgeWeightColumn = "";
                    object.nodeWeightColumn = "";
                    object.edgeColorColumn = "";
                    object.edgeThresholds = null;
                    if ($util.Long) {
                        let long = new $util.Long(0, 0, false);
                        object.edgeLength = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                    } else
                        object.edgeLength = options.longs === String ? "0" : 0;
                    object.enableDefaultHierarchy = false;
                }
                if (message.dotColumn != null && message.hasOwnProperty("dotColumn")) {
                    object.dotColumn = message.dotColumn;
                    if (options.oneofs)
                        object.input = "dotColumn";
                }
                if (message.adjacencyList != null && message.hasOwnProperty("adjacencyList")) {
                    object.adjacencyList = $root.px.vispb.Graph.AdjacencyList.toObject(message.adjacencyList, options);
                    if (options.oneofs)
                        object.input = "adjacencyList";
                }
                if (message.edgeWeightColumn != null && message.hasOwnProperty("edgeWeightColumn"))
                    object.edgeWeightColumn = message.edgeWeightColumn;
                if (message.nodeWeightColumn != null && message.hasOwnProperty("nodeWeightColumn"))
                    object.nodeWeightColumn = message.nodeWeightColumn;
                if (message.edgeColorColumn != null && message.hasOwnProperty("edgeColorColumn"))
                    object.edgeColorColumn = message.edgeColorColumn;
                if (message.edgeThresholds != null && message.hasOwnProperty("edgeThresholds"))
                    object.edgeThresholds = $root.px.vispb.Graph.EdgeThresholds.toObject(message.edgeThresholds, options);
                if (message.edgeHoverInfo && message.edgeHoverInfo.length) {
                    object.edgeHoverInfo = [];
                    for (let j = 0; j < message.edgeHoverInfo.length; ++j)
                        object.edgeHoverInfo[j] = message.edgeHoverInfo[j];
                }
                if (message.edgeLength != null && message.hasOwnProperty("edgeLength"))
                    if (typeof message.edgeLength === "number")
                        object.edgeLength = options.longs === String ? String(message.edgeLength) : message.edgeLength;
                    else
                        object.edgeLength = options.longs === String ? $util.Long.prototype.toString.call(message.edgeLength) : options.longs === Number ? new $util.LongBits(message.edgeLength.low >>> 0, message.edgeLength.high >>> 0).toNumber() : message.edgeLength;
                if (message.enableDefaultHierarchy != null && message.hasOwnProperty("enableDefaultHierarchy"))
                    object.enableDefaultHierarchy = message.enableDefaultHierarchy;
                return object;
            };

            /**
             * Converts this Graph to JSON.
             * @function toJSON
             * @memberof px.vispb.Graph
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Graph.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            Graph.AdjacencyList = (function() {

                /**
                 * Properties of an AdjacencyList.
                 * @memberof px.vispb.Graph
                 * @interface IAdjacencyList
                 * @property {string|null} [fromColumn] AdjacencyList fromColumn
                 * @property {string|null} [toColumn] AdjacencyList toColumn
                 */

                /**
                 * Constructs a new AdjacencyList.
                 * @memberof px.vispb.Graph
                 * @classdesc Represents an AdjacencyList.
                 * @implements IAdjacencyList
                 * @constructor
                 * @param {px.vispb.Graph.IAdjacencyList=} [properties] Properties to set
                 */
                function AdjacencyList(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * AdjacencyList fromColumn.
                 * @member {string} fromColumn
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @instance
                 */
                AdjacencyList.prototype.fromColumn = "";

                /**
                 * AdjacencyList toColumn.
                 * @member {string} toColumn
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @instance
                 */
                AdjacencyList.prototype.toColumn = "";

                /**
                 * Creates a new AdjacencyList instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {px.vispb.Graph.IAdjacencyList=} [properties] Properties to set
                 * @returns {px.vispb.Graph.AdjacencyList} AdjacencyList instance
                 */
                AdjacencyList.create = function create(properties) {
                    return new AdjacencyList(properties);
                };

                /**
                 * Encodes the specified AdjacencyList message. Does not implicitly {@link px.vispb.Graph.AdjacencyList.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {px.vispb.Graph.IAdjacencyList} message AdjacencyList message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                AdjacencyList.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.fromColumn != null && Object.hasOwnProperty.call(message, "fromColumn"))
                        writer.uint32(/* id 1, wireType 2 =*/10).string(message.fromColumn);
                    if (message.toColumn != null && Object.hasOwnProperty.call(message, "toColumn"))
                        writer.uint32(/* id 2, wireType 2 =*/18).string(message.toColumn);
                    return writer;
                };

                /**
                 * Encodes the specified AdjacencyList message, length delimited. Does not implicitly {@link px.vispb.Graph.AdjacencyList.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {px.vispb.Graph.IAdjacencyList} message AdjacencyList message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                AdjacencyList.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes an AdjacencyList message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Graph.AdjacencyList} AdjacencyList
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                AdjacencyList.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Graph.AdjacencyList();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.fromColumn = reader.string();
                            break;
                        case 2:
                            message.toColumn = reader.string();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes an AdjacencyList message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Graph.AdjacencyList} AdjacencyList
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                AdjacencyList.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies an AdjacencyList message.
                 * @function verify
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                AdjacencyList.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.fromColumn != null && message.hasOwnProperty("fromColumn"))
                        if (!$util.isString(message.fromColumn))
                            return "fromColumn: string expected";
                    if (message.toColumn != null && message.hasOwnProperty("toColumn"))
                        if (!$util.isString(message.toColumn))
                            return "toColumn: string expected";
                    return null;
                };

                /**
                 * Creates an AdjacencyList message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Graph.AdjacencyList} AdjacencyList
                 */
                AdjacencyList.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Graph.AdjacencyList)
                        return object;
                    let message = new $root.px.vispb.Graph.AdjacencyList();
                    if (object.fromColumn != null)
                        message.fromColumn = String(object.fromColumn);
                    if (object.toColumn != null)
                        message.toColumn = String(object.toColumn);
                    return message;
                };

                /**
                 * Creates a plain object from an AdjacencyList message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @static
                 * @param {px.vispb.Graph.AdjacencyList} message AdjacencyList
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                AdjacencyList.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        object.fromColumn = "";
                        object.toColumn = "";
                    }
                    if (message.fromColumn != null && message.hasOwnProperty("fromColumn"))
                        object.fromColumn = message.fromColumn;
                    if (message.toColumn != null && message.hasOwnProperty("toColumn"))
                        object.toColumn = message.toColumn;
                    return object;
                };

                /**
                 * Converts this AdjacencyList to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Graph.AdjacencyList
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                AdjacencyList.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return AdjacencyList;
            })();

            Graph.EdgeThresholds = (function() {

                /**
                 * Properties of an EdgeThresholds.
                 * @memberof px.vispb.Graph
                 * @interface IEdgeThresholds
                 * @property {number|Long|null} [mediumThreshold] EdgeThresholds mediumThreshold
                 * @property {number|Long|null} [highThreshold] EdgeThresholds highThreshold
                 */

                /**
                 * Constructs a new EdgeThresholds.
                 * @memberof px.vispb.Graph
                 * @classdesc Represents an EdgeThresholds.
                 * @implements IEdgeThresholds
                 * @constructor
                 * @param {px.vispb.Graph.IEdgeThresholds=} [properties] Properties to set
                 */
                function EdgeThresholds(properties) {
                    if (properties)
                        for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                            if (properties[keys[i]] != null)
                                this[keys[i]] = properties[keys[i]];
                }

                /**
                 * EdgeThresholds mediumThreshold.
                 * @member {number|Long} mediumThreshold
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @instance
                 */
                EdgeThresholds.prototype.mediumThreshold = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

                /**
                 * EdgeThresholds highThreshold.
                 * @member {number|Long} highThreshold
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @instance
                 */
                EdgeThresholds.prototype.highThreshold = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

                /**
                 * Creates a new EdgeThresholds instance using the specified properties.
                 * @function create
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {px.vispb.Graph.IEdgeThresholds=} [properties] Properties to set
                 * @returns {px.vispb.Graph.EdgeThresholds} EdgeThresholds instance
                 */
                EdgeThresholds.create = function create(properties) {
                    return new EdgeThresholds(properties);
                };

                /**
                 * Encodes the specified EdgeThresholds message. Does not implicitly {@link px.vispb.Graph.EdgeThresholds.verify|verify} messages.
                 * @function encode
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {px.vispb.Graph.IEdgeThresholds} message EdgeThresholds message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                EdgeThresholds.encode = function encode(message, writer) {
                    if (!writer)
                        writer = $Writer.create();
                    if (message.mediumThreshold != null && Object.hasOwnProperty.call(message, "mediumThreshold"))
                        writer.uint32(/* id 1, wireType 0 =*/8).int64(message.mediumThreshold);
                    if (message.highThreshold != null && Object.hasOwnProperty.call(message, "highThreshold"))
                        writer.uint32(/* id 2, wireType 0 =*/16).int64(message.highThreshold);
                    return writer;
                };

                /**
                 * Encodes the specified EdgeThresholds message, length delimited. Does not implicitly {@link px.vispb.Graph.EdgeThresholds.verify|verify} messages.
                 * @function encodeDelimited
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {px.vispb.Graph.IEdgeThresholds} message EdgeThresholds message or plain object to encode
                 * @param {$protobuf.Writer} [writer] Writer to encode to
                 * @returns {$protobuf.Writer} Writer
                 */
                EdgeThresholds.encodeDelimited = function encodeDelimited(message, writer) {
                    return this.encode(message, writer).ldelim();
                };

                /**
                 * Decodes an EdgeThresholds message from the specified reader or buffer.
                 * @function decode
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @param {number} [length] Message length if known beforehand
                 * @returns {px.vispb.Graph.EdgeThresholds} EdgeThresholds
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                EdgeThresholds.decode = function decode(reader, length) {
                    if (!(reader instanceof $Reader))
                        reader = $Reader.create(reader);
                    let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.Graph.EdgeThresholds();
                    while (reader.pos < end) {
                        let tag = reader.uint32();
                        switch (tag >>> 3) {
                        case 1:
                            message.mediumThreshold = reader.int64();
                            break;
                        case 2:
                            message.highThreshold = reader.int64();
                            break;
                        default:
                            reader.skipType(tag & 7);
                            break;
                        }
                    }
                    return message;
                };

                /**
                 * Decodes an EdgeThresholds message from the specified reader or buffer, length delimited.
                 * @function decodeDelimited
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                 * @returns {px.vispb.Graph.EdgeThresholds} EdgeThresholds
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                EdgeThresholds.decodeDelimited = function decodeDelimited(reader) {
                    if (!(reader instanceof $Reader))
                        reader = new $Reader(reader);
                    return this.decode(reader, reader.uint32());
                };

                /**
                 * Verifies an EdgeThresholds message.
                 * @function verify
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {Object.<string,*>} message Plain object to verify
                 * @returns {string|null} `null` if valid, otherwise the reason why it is not
                 */
                EdgeThresholds.verify = function verify(message) {
                    if (typeof message !== "object" || message === null)
                        return "object expected";
                    if (message.mediumThreshold != null && message.hasOwnProperty("mediumThreshold"))
                        if (!$util.isInteger(message.mediumThreshold) && !(message.mediumThreshold && $util.isInteger(message.mediumThreshold.low) && $util.isInteger(message.mediumThreshold.high)))
                            return "mediumThreshold: integer|Long expected";
                    if (message.highThreshold != null && message.hasOwnProperty("highThreshold"))
                        if (!$util.isInteger(message.highThreshold) && !(message.highThreshold && $util.isInteger(message.highThreshold.low) && $util.isInteger(message.highThreshold.high)))
                            return "highThreshold: integer|Long expected";
                    return null;
                };

                /**
                 * Creates an EdgeThresholds message from a plain object. Also converts values to their respective internal types.
                 * @function fromObject
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {Object.<string,*>} object Plain object
                 * @returns {px.vispb.Graph.EdgeThresholds} EdgeThresholds
                 */
                EdgeThresholds.fromObject = function fromObject(object) {
                    if (object instanceof $root.px.vispb.Graph.EdgeThresholds)
                        return object;
                    let message = new $root.px.vispb.Graph.EdgeThresholds();
                    if (object.mediumThreshold != null)
                        if ($util.Long)
                            (message.mediumThreshold = $util.Long.fromValue(object.mediumThreshold)).unsigned = false;
                        else if (typeof object.mediumThreshold === "string")
                            message.mediumThreshold = parseInt(object.mediumThreshold, 10);
                        else if (typeof object.mediumThreshold === "number")
                            message.mediumThreshold = object.mediumThreshold;
                        else if (typeof object.mediumThreshold === "object")
                            message.mediumThreshold = new $util.LongBits(object.mediumThreshold.low >>> 0, object.mediumThreshold.high >>> 0).toNumber();
                    if (object.highThreshold != null)
                        if ($util.Long)
                            (message.highThreshold = $util.Long.fromValue(object.highThreshold)).unsigned = false;
                        else if (typeof object.highThreshold === "string")
                            message.highThreshold = parseInt(object.highThreshold, 10);
                        else if (typeof object.highThreshold === "number")
                            message.highThreshold = object.highThreshold;
                        else if (typeof object.highThreshold === "object")
                            message.highThreshold = new $util.LongBits(object.highThreshold.low >>> 0, object.highThreshold.high >>> 0).toNumber();
                    return message;
                };

                /**
                 * Creates a plain object from an EdgeThresholds message. Also converts values to other types if specified.
                 * @function toObject
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @static
                 * @param {px.vispb.Graph.EdgeThresholds} message EdgeThresholds
                 * @param {$protobuf.IConversionOptions} [options] Conversion options
                 * @returns {Object.<string,*>} Plain object
                 */
                EdgeThresholds.toObject = function toObject(message, options) {
                    if (!options)
                        options = {};
                    let object = {};
                    if (options.defaults) {
                        if ($util.Long) {
                            let long = new $util.Long(0, 0, false);
                            object.mediumThreshold = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                        } else
                            object.mediumThreshold = options.longs === String ? "0" : 0;
                        if ($util.Long) {
                            let long = new $util.Long(0, 0, false);
                            object.highThreshold = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                        } else
                            object.highThreshold = options.longs === String ? "0" : 0;
                    }
                    if (message.mediumThreshold != null && message.hasOwnProperty("mediumThreshold"))
                        if (typeof message.mediumThreshold === "number")
                            object.mediumThreshold = options.longs === String ? String(message.mediumThreshold) : message.mediumThreshold;
                        else
                            object.mediumThreshold = options.longs === String ? $util.Long.prototype.toString.call(message.mediumThreshold) : options.longs === Number ? new $util.LongBits(message.mediumThreshold.low >>> 0, message.mediumThreshold.high >>> 0).toNumber() : message.mediumThreshold;
                    if (message.highThreshold != null && message.hasOwnProperty("highThreshold"))
                        if (typeof message.highThreshold === "number")
                            object.highThreshold = options.longs === String ? String(message.highThreshold) : message.highThreshold;
                        else
                            object.highThreshold = options.longs === String ? $util.Long.prototype.toString.call(message.highThreshold) : options.longs === Number ? new $util.LongBits(message.highThreshold.low >>> 0, message.highThreshold.high >>> 0).toNumber() : message.highThreshold;
                    return object;
                };

                /**
                 * Converts this EdgeThresholds to JSON.
                 * @function toJSON
                 * @memberof px.vispb.Graph.EdgeThresholds
                 * @instance
                 * @returns {Object.<string,*>} JSON object
                 */
                EdgeThresholds.prototype.toJSON = function toJSON() {
                    return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                };

                return EdgeThresholds;
            })();

            return Graph;
        })();

        vispb.RequestGraph = (function() {

            /**
             * Properties of a RequestGraph.
             * @memberof px.vispb
             * @interface IRequestGraph
             * @property {string|null} [requestorPodColumn] RequestGraph requestorPodColumn
             * @property {string|null} [responderPodColumn] RequestGraph responderPodColumn
             * @property {string|null} [requestorServiceColumn] RequestGraph requestorServiceColumn
             * @property {string|null} [responderServiceColumn] RequestGraph responderServiceColumn
             * @property {string|null} [requestorIPColumn] RequestGraph requestorIPColumn
             * @property {string|null} [responderIPColumn] RequestGraph responderIPColumn
             * @property {string|null} [p50Column] RequestGraph p50Column
             * @property {string|null} [p90Column] RequestGraph p90Column
             * @property {string|null} [p99Column] RequestGraph p99Column
             * @property {string|null} [errorRateColumn] RequestGraph errorRateColumn
             * @property {string|null} [requestsPerSecondColumn] RequestGraph requestsPerSecondColumn
             * @property {string|null} [inboundBytesPerSecondColumn] RequestGraph inboundBytesPerSecondColumn
             * @property {string|null} [outboundBytesPerSecondColumn] RequestGraph outboundBytesPerSecondColumn
             * @property {string|null} [totalRequestCountColumn] RequestGraph totalRequestCountColumn
             */

            /**
             * Constructs a new RequestGraph.
             * @memberof px.vispb
             * @classdesc Represents a RequestGraph.
             * @implements IRequestGraph
             * @constructor
             * @param {px.vispb.IRequestGraph=} [properties] Properties to set
             */
            function RequestGraph(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * RequestGraph requestorPodColumn.
             * @member {string} requestorPodColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.requestorPodColumn = "";

            /**
             * RequestGraph responderPodColumn.
             * @member {string} responderPodColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.responderPodColumn = "";

            /**
             * RequestGraph requestorServiceColumn.
             * @member {string} requestorServiceColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.requestorServiceColumn = "";

            /**
             * RequestGraph responderServiceColumn.
             * @member {string} responderServiceColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.responderServiceColumn = "";

            /**
             * RequestGraph requestorIPColumn.
             * @member {string} requestorIPColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.requestorIPColumn = "";

            /**
             * RequestGraph responderIPColumn.
             * @member {string} responderIPColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.responderIPColumn = "";

            /**
             * RequestGraph p50Column.
             * @member {string} p50Column
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.p50Column = "";

            /**
             * RequestGraph p90Column.
             * @member {string} p90Column
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.p90Column = "";

            /**
             * RequestGraph p99Column.
             * @member {string} p99Column
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.p99Column = "";

            /**
             * RequestGraph errorRateColumn.
             * @member {string} errorRateColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.errorRateColumn = "";

            /**
             * RequestGraph requestsPerSecondColumn.
             * @member {string} requestsPerSecondColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.requestsPerSecondColumn = "";

            /**
             * RequestGraph inboundBytesPerSecondColumn.
             * @member {string} inboundBytesPerSecondColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.inboundBytesPerSecondColumn = "";

            /**
             * RequestGraph outboundBytesPerSecondColumn.
             * @member {string} outboundBytesPerSecondColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.outboundBytesPerSecondColumn = "";

            /**
             * RequestGraph totalRequestCountColumn.
             * @member {string} totalRequestCountColumn
             * @memberof px.vispb.RequestGraph
             * @instance
             */
            RequestGraph.prototype.totalRequestCountColumn = "";

            /**
             * Creates a new RequestGraph instance using the specified properties.
             * @function create
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {px.vispb.IRequestGraph=} [properties] Properties to set
             * @returns {px.vispb.RequestGraph} RequestGraph instance
             */
            RequestGraph.create = function create(properties) {
                return new RequestGraph(properties);
            };

            /**
             * Encodes the specified RequestGraph message. Does not implicitly {@link px.vispb.RequestGraph.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {px.vispb.IRequestGraph} message RequestGraph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            RequestGraph.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.requestorPodColumn != null && Object.hasOwnProperty.call(message, "requestorPodColumn"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.requestorPodColumn);
                if (message.responderPodColumn != null && Object.hasOwnProperty.call(message, "responderPodColumn"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.responderPodColumn);
                if (message.requestorServiceColumn != null && Object.hasOwnProperty.call(message, "requestorServiceColumn"))
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.requestorServiceColumn);
                if (message.responderServiceColumn != null && Object.hasOwnProperty.call(message, "responderServiceColumn"))
                    writer.uint32(/* id 4, wireType 2 =*/34).string(message.responderServiceColumn);
                if (message.p50Column != null && Object.hasOwnProperty.call(message, "p50Column"))
                    writer.uint32(/* id 5, wireType 2 =*/42).string(message.p50Column);
                if (message.p90Column != null && Object.hasOwnProperty.call(message, "p90Column"))
                    writer.uint32(/* id 6, wireType 2 =*/50).string(message.p90Column);
                if (message.p99Column != null && Object.hasOwnProperty.call(message, "p99Column"))
                    writer.uint32(/* id 7, wireType 2 =*/58).string(message.p99Column);
                if (message.errorRateColumn != null && Object.hasOwnProperty.call(message, "errorRateColumn"))
                    writer.uint32(/* id 8, wireType 2 =*/66).string(message.errorRateColumn);
                if (message.requestsPerSecondColumn != null && Object.hasOwnProperty.call(message, "requestsPerSecondColumn"))
                    writer.uint32(/* id 9, wireType 2 =*/74).string(message.requestsPerSecondColumn);
                if (message.inboundBytesPerSecondColumn != null && Object.hasOwnProperty.call(message, "inboundBytesPerSecondColumn"))
                    writer.uint32(/* id 10, wireType 2 =*/82).string(message.inboundBytesPerSecondColumn);
                if (message.outboundBytesPerSecondColumn != null && Object.hasOwnProperty.call(message, "outboundBytesPerSecondColumn"))
                    writer.uint32(/* id 11, wireType 2 =*/90).string(message.outboundBytesPerSecondColumn);
                if (message.totalRequestCountColumn != null && Object.hasOwnProperty.call(message, "totalRequestCountColumn"))
                    writer.uint32(/* id 12, wireType 2 =*/98).string(message.totalRequestCountColumn);
                if (message.requestorIPColumn != null && Object.hasOwnProperty.call(message, "requestorIPColumn"))
                    writer.uint32(/* id 13, wireType 2 =*/106).string(message.requestorIPColumn);
                if (message.responderIPColumn != null && Object.hasOwnProperty.call(message, "responderIPColumn"))
                    writer.uint32(/* id 14, wireType 2 =*/114).string(message.responderIPColumn);
                return writer;
            };

            /**
             * Encodes the specified RequestGraph message, length delimited. Does not implicitly {@link px.vispb.RequestGraph.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {px.vispb.IRequestGraph} message RequestGraph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            RequestGraph.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a RequestGraph message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.RequestGraph} RequestGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            RequestGraph.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.RequestGraph();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.requestorPodColumn = reader.string();
                        break;
                    case 2:
                        message.responderPodColumn = reader.string();
                        break;
                    case 3:
                        message.requestorServiceColumn = reader.string();
                        break;
                    case 4:
                        message.responderServiceColumn = reader.string();
                        break;
                    case 13:
                        message.requestorIPColumn = reader.string();
                        break;
                    case 14:
                        message.responderIPColumn = reader.string();
                        break;
                    case 5:
                        message.p50Column = reader.string();
                        break;
                    case 6:
                        message.p90Column = reader.string();
                        break;
                    case 7:
                        message.p99Column = reader.string();
                        break;
                    case 8:
                        message.errorRateColumn = reader.string();
                        break;
                    case 9:
                        message.requestsPerSecondColumn = reader.string();
                        break;
                    case 10:
                        message.inboundBytesPerSecondColumn = reader.string();
                        break;
                    case 11:
                        message.outboundBytesPerSecondColumn = reader.string();
                        break;
                    case 12:
                        message.totalRequestCountColumn = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a RequestGraph message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.RequestGraph} RequestGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            RequestGraph.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a RequestGraph message.
             * @function verify
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            RequestGraph.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.requestorPodColumn != null && message.hasOwnProperty("requestorPodColumn"))
                    if (!$util.isString(message.requestorPodColumn))
                        return "requestorPodColumn: string expected";
                if (message.responderPodColumn != null && message.hasOwnProperty("responderPodColumn"))
                    if (!$util.isString(message.responderPodColumn))
                        return "responderPodColumn: string expected";
                if (message.requestorServiceColumn != null && message.hasOwnProperty("requestorServiceColumn"))
                    if (!$util.isString(message.requestorServiceColumn))
                        return "requestorServiceColumn: string expected";
                if (message.responderServiceColumn != null && message.hasOwnProperty("responderServiceColumn"))
                    if (!$util.isString(message.responderServiceColumn))
                        return "responderServiceColumn: string expected";
                if (message.requestorIPColumn != null && message.hasOwnProperty("requestorIPColumn"))
                    if (!$util.isString(message.requestorIPColumn))
                        return "requestorIPColumn: string expected";
                if (message.responderIPColumn != null && message.hasOwnProperty("responderIPColumn"))
                    if (!$util.isString(message.responderIPColumn))
                        return "responderIPColumn: string expected";
                if (message.p50Column != null && message.hasOwnProperty("p50Column"))
                    if (!$util.isString(message.p50Column))
                        return "p50Column: string expected";
                if (message.p90Column != null && message.hasOwnProperty("p90Column"))
                    if (!$util.isString(message.p90Column))
                        return "p90Column: string expected";
                if (message.p99Column != null && message.hasOwnProperty("p99Column"))
                    if (!$util.isString(message.p99Column))
                        return "p99Column: string expected";
                if (message.errorRateColumn != null && message.hasOwnProperty("errorRateColumn"))
                    if (!$util.isString(message.errorRateColumn))
                        return "errorRateColumn: string expected";
                if (message.requestsPerSecondColumn != null && message.hasOwnProperty("requestsPerSecondColumn"))
                    if (!$util.isString(message.requestsPerSecondColumn))
                        return "requestsPerSecondColumn: string expected";
                if (message.inboundBytesPerSecondColumn != null && message.hasOwnProperty("inboundBytesPerSecondColumn"))
                    if (!$util.isString(message.inboundBytesPerSecondColumn))
                        return "inboundBytesPerSecondColumn: string expected";
                if (message.outboundBytesPerSecondColumn != null && message.hasOwnProperty("outboundBytesPerSecondColumn"))
                    if (!$util.isString(message.outboundBytesPerSecondColumn))
                        return "outboundBytesPerSecondColumn: string expected";
                if (message.totalRequestCountColumn != null && message.hasOwnProperty("totalRequestCountColumn"))
                    if (!$util.isString(message.totalRequestCountColumn))
                        return "totalRequestCountColumn: string expected";
                return null;
            };

            /**
             * Creates a RequestGraph message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.RequestGraph} RequestGraph
             */
            RequestGraph.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.RequestGraph)
                    return object;
                let message = new $root.px.vispb.RequestGraph();
                if (object.requestorPodColumn != null)
                    message.requestorPodColumn = String(object.requestorPodColumn);
                if (object.responderPodColumn != null)
                    message.responderPodColumn = String(object.responderPodColumn);
                if (object.requestorServiceColumn != null)
                    message.requestorServiceColumn = String(object.requestorServiceColumn);
                if (object.responderServiceColumn != null)
                    message.responderServiceColumn = String(object.responderServiceColumn);
                if (object.requestorIPColumn != null)
                    message.requestorIPColumn = String(object.requestorIPColumn);
                if (object.responderIPColumn != null)
                    message.responderIPColumn = String(object.responderIPColumn);
                if (object.p50Column != null)
                    message.p50Column = String(object.p50Column);
                if (object.p90Column != null)
                    message.p90Column = String(object.p90Column);
                if (object.p99Column != null)
                    message.p99Column = String(object.p99Column);
                if (object.errorRateColumn != null)
                    message.errorRateColumn = String(object.errorRateColumn);
                if (object.requestsPerSecondColumn != null)
                    message.requestsPerSecondColumn = String(object.requestsPerSecondColumn);
                if (object.inboundBytesPerSecondColumn != null)
                    message.inboundBytesPerSecondColumn = String(object.inboundBytesPerSecondColumn);
                if (object.outboundBytesPerSecondColumn != null)
                    message.outboundBytesPerSecondColumn = String(object.outboundBytesPerSecondColumn);
                if (object.totalRequestCountColumn != null)
                    message.totalRequestCountColumn = String(object.totalRequestCountColumn);
                return message;
            };

            /**
             * Creates a plain object from a RequestGraph message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.RequestGraph
             * @static
             * @param {px.vispb.RequestGraph} message RequestGraph
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            RequestGraph.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.requestorPodColumn = "";
                    object.responderPodColumn = "";
                    object.requestorServiceColumn = "";
                    object.responderServiceColumn = "";
                    object.p50Column = "";
                    object.p90Column = "";
                    object.p99Column = "";
                    object.errorRateColumn = "";
                    object.requestsPerSecondColumn = "";
                    object.inboundBytesPerSecondColumn = "";
                    object.outboundBytesPerSecondColumn = "";
                    object.totalRequestCountColumn = "";
                    object.requestorIPColumn = "";
                    object.responderIPColumn = "";
                }
                if (message.requestorPodColumn != null && message.hasOwnProperty("requestorPodColumn"))
                    object.requestorPodColumn = message.requestorPodColumn;
                if (message.responderPodColumn != null && message.hasOwnProperty("responderPodColumn"))
                    object.responderPodColumn = message.responderPodColumn;
                if (message.requestorServiceColumn != null && message.hasOwnProperty("requestorServiceColumn"))
                    object.requestorServiceColumn = message.requestorServiceColumn;
                if (message.responderServiceColumn != null && message.hasOwnProperty("responderServiceColumn"))
                    object.responderServiceColumn = message.responderServiceColumn;
                if (message.p50Column != null && message.hasOwnProperty("p50Column"))
                    object.p50Column = message.p50Column;
                if (message.p90Column != null && message.hasOwnProperty("p90Column"))
                    object.p90Column = message.p90Column;
                if (message.p99Column != null && message.hasOwnProperty("p99Column"))
                    object.p99Column = message.p99Column;
                if (message.errorRateColumn != null && message.hasOwnProperty("errorRateColumn"))
                    object.errorRateColumn = message.errorRateColumn;
                if (message.requestsPerSecondColumn != null && message.hasOwnProperty("requestsPerSecondColumn"))
                    object.requestsPerSecondColumn = message.requestsPerSecondColumn;
                if (message.inboundBytesPerSecondColumn != null && message.hasOwnProperty("inboundBytesPerSecondColumn"))
                    object.inboundBytesPerSecondColumn = message.inboundBytesPerSecondColumn;
                if (message.outboundBytesPerSecondColumn != null && message.hasOwnProperty("outboundBytesPerSecondColumn"))
                    object.outboundBytesPerSecondColumn = message.outboundBytesPerSecondColumn;
                if (message.totalRequestCountColumn != null && message.hasOwnProperty("totalRequestCountColumn"))
                    object.totalRequestCountColumn = message.totalRequestCountColumn;
                if (message.requestorIPColumn != null && message.hasOwnProperty("requestorIPColumn"))
                    object.requestorIPColumn = message.requestorIPColumn;
                if (message.responderIPColumn != null && message.hasOwnProperty("responderIPColumn"))
                    object.responderIPColumn = message.responderIPColumn;
                return object;
            };

            /**
             * Converts this RequestGraph to JSON.
             * @function toJSON
             * @memberof px.vispb.RequestGraph
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            RequestGraph.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return RequestGraph;
        })();

        vispb.StackTraceFlameGraph = (function() {

            /**
             * Properties of a StackTraceFlameGraph.
             * @memberof px.vispb
             * @interface IStackTraceFlameGraph
             * @property {string|null} [stacktraceColumn] StackTraceFlameGraph stacktraceColumn
             * @property {string|null} [countColumn] StackTraceFlameGraph countColumn
             * @property {string|null} [percentageColumn] StackTraceFlameGraph percentageColumn
             * @property {string|null} [namespaceColumn] StackTraceFlameGraph namespaceColumn
             * @property {string|null} [podColumn] StackTraceFlameGraph podColumn
             * @property {string|null} [containerColumn] StackTraceFlameGraph containerColumn
             * @property {string|null} [pidColumn] StackTraceFlameGraph pidColumn
             * @property {string|null} [nodeColumn] StackTraceFlameGraph nodeColumn
             * @property {string|null} [percentageLabel] StackTraceFlameGraph percentageLabel
             */

            /**
             * Constructs a new StackTraceFlameGraph.
             * @memberof px.vispb
             * @classdesc Represents a StackTraceFlameGraph.
             * @implements IStackTraceFlameGraph
             * @constructor
             * @param {px.vispb.IStackTraceFlameGraph=} [properties] Properties to set
             */
            function StackTraceFlameGraph(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * StackTraceFlameGraph stacktraceColumn.
             * @member {string} stacktraceColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.stacktraceColumn = "";

            /**
             * StackTraceFlameGraph countColumn.
             * @member {string} countColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.countColumn = "";

            /**
             * StackTraceFlameGraph percentageColumn.
             * @member {string} percentageColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.percentageColumn = "";

            /**
             * StackTraceFlameGraph namespaceColumn.
             * @member {string} namespaceColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.namespaceColumn = "";

            /**
             * StackTraceFlameGraph podColumn.
             * @member {string} podColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.podColumn = "";

            /**
             * StackTraceFlameGraph containerColumn.
             * @member {string} containerColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.containerColumn = "";

            /**
             * StackTraceFlameGraph pidColumn.
             * @member {string} pidColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.pidColumn = "";

            /**
             * StackTraceFlameGraph nodeColumn.
             * @member {string} nodeColumn
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.nodeColumn = "";

            /**
             * StackTraceFlameGraph percentageLabel.
             * @member {string} percentageLabel
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             */
            StackTraceFlameGraph.prototype.percentageLabel = "";

            /**
             * Creates a new StackTraceFlameGraph instance using the specified properties.
             * @function create
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {px.vispb.IStackTraceFlameGraph=} [properties] Properties to set
             * @returns {px.vispb.StackTraceFlameGraph} StackTraceFlameGraph instance
             */
            StackTraceFlameGraph.create = function create(properties) {
                return new StackTraceFlameGraph(properties);
            };

            /**
             * Encodes the specified StackTraceFlameGraph message. Does not implicitly {@link px.vispb.StackTraceFlameGraph.verify|verify} messages.
             * @function encode
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {px.vispb.IStackTraceFlameGraph} message StackTraceFlameGraph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StackTraceFlameGraph.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.stacktraceColumn != null && Object.hasOwnProperty.call(message, "stacktraceColumn"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.stacktraceColumn);
                if (message.countColumn != null && Object.hasOwnProperty.call(message, "countColumn"))
                    writer.uint32(/* id 2, wireType 2 =*/18).string(message.countColumn);
                if (message.percentageColumn != null && Object.hasOwnProperty.call(message, "percentageColumn"))
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.percentageColumn);
                if (message.namespaceColumn != null && Object.hasOwnProperty.call(message, "namespaceColumn"))
                    writer.uint32(/* id 4, wireType 2 =*/34).string(message.namespaceColumn);
                if (message.podColumn != null && Object.hasOwnProperty.call(message, "podColumn"))
                    writer.uint32(/* id 5, wireType 2 =*/42).string(message.podColumn);
                if (message.containerColumn != null && Object.hasOwnProperty.call(message, "containerColumn"))
                    writer.uint32(/* id 6, wireType 2 =*/50).string(message.containerColumn);
                if (message.pidColumn != null && Object.hasOwnProperty.call(message, "pidColumn"))
                    writer.uint32(/* id 7, wireType 2 =*/58).string(message.pidColumn);
                if (message.nodeColumn != null && Object.hasOwnProperty.call(message, "nodeColumn"))
                    writer.uint32(/* id 8, wireType 2 =*/66).string(message.nodeColumn);
                if (message.percentageLabel != null && Object.hasOwnProperty.call(message, "percentageLabel"))
                    writer.uint32(/* id 9, wireType 2 =*/74).string(message.percentageLabel);
                return writer;
            };

            /**
             * Encodes the specified StackTraceFlameGraph message, length delimited. Does not implicitly {@link px.vispb.StackTraceFlameGraph.verify|verify} messages.
             * @function encodeDelimited
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {px.vispb.IStackTraceFlameGraph} message StackTraceFlameGraph message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StackTraceFlameGraph.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a StackTraceFlameGraph message from the specified reader or buffer.
             * @function decode
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {px.vispb.StackTraceFlameGraph} StackTraceFlameGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StackTraceFlameGraph.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.px.vispb.StackTraceFlameGraph();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.stacktraceColumn = reader.string();
                        break;
                    case 2:
                        message.countColumn = reader.string();
                        break;
                    case 3:
                        message.percentageColumn = reader.string();
                        break;
                    case 4:
                        message.namespaceColumn = reader.string();
                        break;
                    case 5:
                        message.podColumn = reader.string();
                        break;
                    case 6:
                        message.containerColumn = reader.string();
                        break;
                    case 7:
                        message.pidColumn = reader.string();
                        break;
                    case 8:
                        message.nodeColumn = reader.string();
                        break;
                    case 9:
                        message.percentageLabel = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a StackTraceFlameGraph message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {px.vispb.StackTraceFlameGraph} StackTraceFlameGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StackTraceFlameGraph.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a StackTraceFlameGraph message.
             * @function verify
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            StackTraceFlameGraph.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.stacktraceColumn != null && message.hasOwnProperty("stacktraceColumn"))
                    if (!$util.isString(message.stacktraceColumn))
                        return "stacktraceColumn: string expected";
                if (message.countColumn != null && message.hasOwnProperty("countColumn"))
                    if (!$util.isString(message.countColumn))
                        return "countColumn: string expected";
                if (message.percentageColumn != null && message.hasOwnProperty("percentageColumn"))
                    if (!$util.isString(message.percentageColumn))
                        return "percentageColumn: string expected";
                if (message.namespaceColumn != null && message.hasOwnProperty("namespaceColumn"))
                    if (!$util.isString(message.namespaceColumn))
                        return "namespaceColumn: string expected";
                if (message.podColumn != null && message.hasOwnProperty("podColumn"))
                    if (!$util.isString(message.podColumn))
                        return "podColumn: string expected";
                if (message.containerColumn != null && message.hasOwnProperty("containerColumn"))
                    if (!$util.isString(message.containerColumn))
                        return "containerColumn: string expected";
                if (message.pidColumn != null && message.hasOwnProperty("pidColumn"))
                    if (!$util.isString(message.pidColumn))
                        return "pidColumn: string expected";
                if (message.nodeColumn != null && message.hasOwnProperty("nodeColumn"))
                    if (!$util.isString(message.nodeColumn))
                        return "nodeColumn: string expected";
                if (message.percentageLabel != null && message.hasOwnProperty("percentageLabel"))
                    if (!$util.isString(message.percentageLabel))
                        return "percentageLabel: string expected";
                return null;
            };

            /**
             * Creates a StackTraceFlameGraph message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {px.vispb.StackTraceFlameGraph} StackTraceFlameGraph
             */
            StackTraceFlameGraph.fromObject = function fromObject(object) {
                if (object instanceof $root.px.vispb.StackTraceFlameGraph)
                    return object;
                let message = new $root.px.vispb.StackTraceFlameGraph();
                if (object.stacktraceColumn != null)
                    message.stacktraceColumn = String(object.stacktraceColumn);
                if (object.countColumn != null)
                    message.countColumn = String(object.countColumn);
                if (object.percentageColumn != null)
                    message.percentageColumn = String(object.percentageColumn);
                if (object.namespaceColumn != null)
                    message.namespaceColumn = String(object.namespaceColumn);
                if (object.podColumn != null)
                    message.podColumn = String(object.podColumn);
                if (object.containerColumn != null)
                    message.containerColumn = String(object.containerColumn);
                if (object.pidColumn != null)
                    message.pidColumn = String(object.pidColumn);
                if (object.nodeColumn != null)
                    message.nodeColumn = String(object.nodeColumn);
                if (object.percentageLabel != null)
                    message.percentageLabel = String(object.percentageLabel);
                return message;
            };

            /**
             * Creates a plain object from a StackTraceFlameGraph message. Also converts values to other types if specified.
             * @function toObject
             * @memberof px.vispb.StackTraceFlameGraph
             * @static
             * @param {px.vispb.StackTraceFlameGraph} message StackTraceFlameGraph
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            StackTraceFlameGraph.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.stacktraceColumn = "";
                    object.countColumn = "";
                    object.percentageColumn = "";
                    object.namespaceColumn = "";
                    object.podColumn = "";
                    object.containerColumn = "";
                    object.pidColumn = "";
                    object.nodeColumn = "";
                    object.percentageLabel = "";
                }
                if (message.stacktraceColumn != null && message.hasOwnProperty("stacktraceColumn"))
                    object.stacktraceColumn = message.stacktraceColumn;
                if (message.countColumn != null && message.hasOwnProperty("countColumn"))
                    object.countColumn = message.countColumn;
                if (message.percentageColumn != null && message.hasOwnProperty("percentageColumn"))
                    object.percentageColumn = message.percentageColumn;
                if (message.namespaceColumn != null && message.hasOwnProperty("namespaceColumn"))
                    object.namespaceColumn = message.namespaceColumn;
                if (message.podColumn != null && message.hasOwnProperty("podColumn"))
                    object.podColumn = message.podColumn;
                if (message.containerColumn != null && message.hasOwnProperty("containerColumn"))
                    object.containerColumn = message.containerColumn;
                if (message.pidColumn != null && message.hasOwnProperty("pidColumn"))
                    object.pidColumn = message.pidColumn;
                if (message.nodeColumn != null && message.hasOwnProperty("nodeColumn"))
                    object.nodeColumn = message.nodeColumn;
                if (message.percentageLabel != null && message.hasOwnProperty("percentageLabel"))
                    object.percentageLabel = message.percentageLabel;
                return object;
            };

            /**
             * Converts this StackTraceFlameGraph to JSON.
             * @function toJSON
             * @memberof px.vispb.StackTraceFlameGraph
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            StackTraceFlameGraph.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return StackTraceFlameGraph;
        })();

        return vispb;
    })();

    return px;
})();

export const google = $root.google = (() => {

    /**
     * Namespace google.
     * @exports google
     * @namespace
     */
    const google = {};

    google.protobuf = (function() {

        /**
         * Namespace protobuf.
         * @memberof google
         * @namespace
         */
        const protobuf = {};

        protobuf.Any = (function() {

            /**
             * Properties of an Any.
             * @memberof google.protobuf
             * @interface IAny
             * @property {string|null} [type_url] Any type_url
             * @property {Uint8Array|null} [value] Any value
             */

            /**
             * Constructs a new Any.
             * @memberof google.protobuf
             * @classdesc Represents an Any.
             * @implements IAny
             * @constructor
             * @param {google.protobuf.IAny=} [properties] Properties to set
             */
            function Any(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Any type_url.
             * @member {string} type_url
             * @memberof google.protobuf.Any
             * @instance
             */
            Any.prototype.type_url = "";

            /**
             * Any value.
             * @member {Uint8Array} value
             * @memberof google.protobuf.Any
             * @instance
             */
            Any.prototype.value = $util.newBuffer([]);

            /**
             * Creates a new Any instance using the specified properties.
             * @function create
             * @memberof google.protobuf.Any
             * @static
             * @param {google.protobuf.IAny=} [properties] Properties to set
             * @returns {google.protobuf.Any} Any instance
             */
            Any.create = function create(properties) {
                return new Any(properties);
            };

            /**
             * Encodes the specified Any message. Does not implicitly {@link google.protobuf.Any.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.Any
             * @static
             * @param {google.protobuf.IAny} message Any message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Any.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.type_url != null && Object.hasOwnProperty.call(message, "type_url"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.type_url);
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 2, wireType 2 =*/18).bytes(message.value);
                return writer;
            };

            /**
             * Encodes the specified Any message, length delimited. Does not implicitly {@link google.protobuf.Any.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.Any
             * @static
             * @param {google.protobuf.IAny} message Any message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Any.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes an Any message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.Any
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.Any} Any
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Any.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.Any();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.type_url = reader.string();
                        break;
                    case 2:
                        message.value = reader.bytes();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes an Any message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.Any
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.Any} Any
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Any.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies an Any message.
             * @function verify
             * @memberof google.protobuf.Any
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Any.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.type_url != null && message.hasOwnProperty("type_url"))
                    if (!$util.isString(message.type_url))
                        return "type_url: string expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!(message.value && typeof message.value.length === "number" || $util.isString(message.value)))
                        return "value: buffer expected";
                return null;
            };

            /**
             * Creates an Any message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.Any
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.Any} Any
             */
            Any.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.Any)
                    return object;
                let message = new $root.google.protobuf.Any();
                if (object.type_url != null)
                    message.type_url = String(object.type_url);
                if (object.value != null)
                    if (typeof object.value === "string")
                        $util.base64.decode(object.value, message.value = $util.newBuffer($util.base64.length(object.value)), 0);
                    else if (object.value.length)
                        message.value = object.value;
                return message;
            };

            /**
             * Creates a plain object from an Any message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.Any
             * @static
             * @param {google.protobuf.Any} message Any
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Any.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults) {
                    object.type_url = "";
                    if (options.bytes === String)
                        object.value = "";
                    else {
                        object.value = [];
                        if (options.bytes !== Array)
                            object.value = $util.newBuffer(object.value);
                    }
                }
                if (message.type_url != null && message.hasOwnProperty("type_url"))
                    object.type_url = message.type_url;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = options.bytes === String ? $util.base64.encode(message.value, 0, message.value.length) : options.bytes === Array ? Array.prototype.slice.call(message.value) : message.value;
                return object;
            };

            /**
             * Converts this Any to JSON.
             * @function toJSON
             * @memberof google.protobuf.Any
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Any.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return Any;
        })();

        protobuf.DoubleValue = (function() {

            /**
             * Properties of a DoubleValue.
             * @memberof google.protobuf
             * @interface IDoubleValue
             * @property {number|null} [value] DoubleValue value
             */

            /**
             * Constructs a new DoubleValue.
             * @memberof google.protobuf
             * @classdesc Represents a DoubleValue.
             * @implements IDoubleValue
             * @constructor
             * @param {google.protobuf.IDoubleValue=} [properties] Properties to set
             */
            function DoubleValue(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * DoubleValue value.
             * @member {number} value
             * @memberof google.protobuf.DoubleValue
             * @instance
             */
            DoubleValue.prototype.value = 0;

            /**
             * Creates a new DoubleValue instance using the specified properties.
             * @function create
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {google.protobuf.IDoubleValue=} [properties] Properties to set
             * @returns {google.protobuf.DoubleValue} DoubleValue instance
             */
            DoubleValue.create = function create(properties) {
                return new DoubleValue(properties);
            };

            /**
             * Encodes the specified DoubleValue message. Does not implicitly {@link google.protobuf.DoubleValue.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {google.protobuf.IDoubleValue} message DoubleValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            DoubleValue.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 1 =*/9).double(message.value);
                return writer;
            };

            /**
             * Encodes the specified DoubleValue message, length delimited. Does not implicitly {@link google.protobuf.DoubleValue.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {google.protobuf.IDoubleValue} message DoubleValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            DoubleValue.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a DoubleValue message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.DoubleValue} DoubleValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            DoubleValue.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.DoubleValue();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.double();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a DoubleValue message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.DoubleValue} DoubleValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            DoubleValue.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a DoubleValue message.
             * @function verify
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            DoubleValue.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (typeof message.value !== "number")
                        return "value: number expected";
                return null;
            };

            /**
             * Creates a DoubleValue message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.DoubleValue} DoubleValue
             */
            DoubleValue.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.DoubleValue)
                    return object;
                let message = new $root.google.protobuf.DoubleValue();
                if (object.value != null)
                    message.value = Number(object.value);
                return message;
            };

            /**
             * Creates a plain object from a DoubleValue message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.DoubleValue
             * @static
             * @param {google.protobuf.DoubleValue} message DoubleValue
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            DoubleValue.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = options.json && !isFinite(message.value) ? String(message.value) : message.value;
                return object;
            };

            /**
             * Converts this DoubleValue to JSON.
             * @function toJSON
             * @memberof google.protobuf.DoubleValue
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            DoubleValue.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return DoubleValue;
        })();

        protobuf.FloatValue = (function() {

            /**
             * Properties of a FloatValue.
             * @memberof google.protobuf
             * @interface IFloatValue
             * @property {number|null} [value] FloatValue value
             */

            /**
             * Constructs a new FloatValue.
             * @memberof google.protobuf
             * @classdesc Represents a FloatValue.
             * @implements IFloatValue
             * @constructor
             * @param {google.protobuf.IFloatValue=} [properties] Properties to set
             */
            function FloatValue(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * FloatValue value.
             * @member {number} value
             * @memberof google.protobuf.FloatValue
             * @instance
             */
            FloatValue.prototype.value = 0;

            /**
             * Creates a new FloatValue instance using the specified properties.
             * @function create
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {google.protobuf.IFloatValue=} [properties] Properties to set
             * @returns {google.protobuf.FloatValue} FloatValue instance
             */
            FloatValue.create = function create(properties) {
                return new FloatValue(properties);
            };

            /**
             * Encodes the specified FloatValue message. Does not implicitly {@link google.protobuf.FloatValue.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {google.protobuf.IFloatValue} message FloatValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            FloatValue.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 5 =*/13).float(message.value);
                return writer;
            };

            /**
             * Encodes the specified FloatValue message, length delimited. Does not implicitly {@link google.protobuf.FloatValue.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {google.protobuf.IFloatValue} message FloatValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            FloatValue.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a FloatValue message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.FloatValue} FloatValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            FloatValue.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.FloatValue();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.float();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a FloatValue message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.FloatValue} FloatValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            FloatValue.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a FloatValue message.
             * @function verify
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            FloatValue.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (typeof message.value !== "number")
                        return "value: number expected";
                return null;
            };

            /**
             * Creates a FloatValue message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.FloatValue} FloatValue
             */
            FloatValue.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.FloatValue)
                    return object;
                let message = new $root.google.protobuf.FloatValue();
                if (object.value != null)
                    message.value = Number(object.value);
                return message;
            };

            /**
             * Creates a plain object from a FloatValue message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.FloatValue
             * @static
             * @param {google.protobuf.FloatValue} message FloatValue
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            FloatValue.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = options.json && !isFinite(message.value) ? String(message.value) : message.value;
                return object;
            };

            /**
             * Converts this FloatValue to JSON.
             * @function toJSON
             * @memberof google.protobuf.FloatValue
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            FloatValue.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return FloatValue;
        })();

        protobuf.Int64Value = (function() {

            /**
             * Properties of an Int64Value.
             * @memberof google.protobuf
             * @interface IInt64Value
             * @property {number|Long|null} [value] Int64Value value
             */

            /**
             * Constructs a new Int64Value.
             * @memberof google.protobuf
             * @classdesc Represents an Int64Value.
             * @implements IInt64Value
             * @constructor
             * @param {google.protobuf.IInt64Value=} [properties] Properties to set
             */
            function Int64Value(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Int64Value value.
             * @member {number|Long} value
             * @memberof google.protobuf.Int64Value
             * @instance
             */
            Int64Value.prototype.value = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

            /**
             * Creates a new Int64Value instance using the specified properties.
             * @function create
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {google.protobuf.IInt64Value=} [properties] Properties to set
             * @returns {google.protobuf.Int64Value} Int64Value instance
             */
            Int64Value.create = function create(properties) {
                return new Int64Value(properties);
            };

            /**
             * Encodes the specified Int64Value message. Does not implicitly {@link google.protobuf.Int64Value.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {google.protobuf.IInt64Value} message Int64Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Int64Value.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 0 =*/8).int64(message.value);
                return writer;
            };

            /**
             * Encodes the specified Int64Value message, length delimited. Does not implicitly {@link google.protobuf.Int64Value.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {google.protobuf.IInt64Value} message Int64Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Int64Value.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes an Int64Value message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.Int64Value} Int64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Int64Value.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.Int64Value();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.int64();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes an Int64Value message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.Int64Value} Int64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Int64Value.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies an Int64Value message.
             * @function verify
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Int64Value.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isInteger(message.value) && !(message.value && $util.isInteger(message.value.low) && $util.isInteger(message.value.high)))
                        return "value: integer|Long expected";
                return null;
            };

            /**
             * Creates an Int64Value message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.Int64Value} Int64Value
             */
            Int64Value.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.Int64Value)
                    return object;
                let message = new $root.google.protobuf.Int64Value();
                if (object.value != null)
                    if ($util.Long)
                        (message.value = $util.Long.fromValue(object.value)).unsigned = false;
                    else if (typeof object.value === "string")
                        message.value = parseInt(object.value, 10);
                    else if (typeof object.value === "number")
                        message.value = object.value;
                    else if (typeof object.value === "object")
                        message.value = new $util.LongBits(object.value.low >>> 0, object.value.high >>> 0).toNumber();
                return message;
            };

            /**
             * Creates a plain object from an Int64Value message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.Int64Value
             * @static
             * @param {google.protobuf.Int64Value} message Int64Value
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Int64Value.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    if ($util.Long) {
                        let long = new $util.Long(0, 0, false);
                        object.value = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                    } else
                        object.value = options.longs === String ? "0" : 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    if (typeof message.value === "number")
                        object.value = options.longs === String ? String(message.value) : message.value;
                    else
                        object.value = options.longs === String ? $util.Long.prototype.toString.call(message.value) : options.longs === Number ? new $util.LongBits(message.value.low >>> 0, message.value.high >>> 0).toNumber() : message.value;
                return object;
            };

            /**
             * Converts this Int64Value to JSON.
             * @function toJSON
             * @memberof google.protobuf.Int64Value
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Int64Value.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return Int64Value;
        })();

        protobuf.UInt64Value = (function() {

            /**
             * Properties of a UInt64Value.
             * @memberof google.protobuf
             * @interface IUInt64Value
             * @property {number|Long|null} [value] UInt64Value value
             */

            /**
             * Constructs a new UInt64Value.
             * @memberof google.protobuf
             * @classdesc Represents a UInt64Value.
             * @implements IUInt64Value
             * @constructor
             * @param {google.protobuf.IUInt64Value=} [properties] Properties to set
             */
            function UInt64Value(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * UInt64Value value.
             * @member {number|Long} value
             * @memberof google.protobuf.UInt64Value
             * @instance
             */
            UInt64Value.prototype.value = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

            /**
             * Creates a new UInt64Value instance using the specified properties.
             * @function create
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {google.protobuf.IUInt64Value=} [properties] Properties to set
             * @returns {google.protobuf.UInt64Value} UInt64Value instance
             */
            UInt64Value.create = function create(properties) {
                return new UInt64Value(properties);
            };

            /**
             * Encodes the specified UInt64Value message. Does not implicitly {@link google.protobuf.UInt64Value.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {google.protobuf.IUInt64Value} message UInt64Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            UInt64Value.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.value);
                return writer;
            };

            /**
             * Encodes the specified UInt64Value message, length delimited. Does not implicitly {@link google.protobuf.UInt64Value.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {google.protobuf.IUInt64Value} message UInt64Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            UInt64Value.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a UInt64Value message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.UInt64Value} UInt64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            UInt64Value.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.UInt64Value();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.uint64();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a UInt64Value message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.UInt64Value} UInt64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            UInt64Value.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a UInt64Value message.
             * @function verify
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            UInt64Value.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isInteger(message.value) && !(message.value && $util.isInteger(message.value.low) && $util.isInteger(message.value.high)))
                        return "value: integer|Long expected";
                return null;
            };

            /**
             * Creates a UInt64Value message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.UInt64Value} UInt64Value
             */
            UInt64Value.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.UInt64Value)
                    return object;
                let message = new $root.google.protobuf.UInt64Value();
                if (object.value != null)
                    if ($util.Long)
                        (message.value = $util.Long.fromValue(object.value)).unsigned = true;
                    else if (typeof object.value === "string")
                        message.value = parseInt(object.value, 10);
                    else if (typeof object.value === "number")
                        message.value = object.value;
                    else if (typeof object.value === "object")
                        message.value = new $util.LongBits(object.value.low >>> 0, object.value.high >>> 0).toNumber(true);
                return message;
            };

            /**
             * Creates a plain object from a UInt64Value message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.UInt64Value
             * @static
             * @param {google.protobuf.UInt64Value} message UInt64Value
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            UInt64Value.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    if ($util.Long) {
                        let long = new $util.Long(0, 0, true);
                        object.value = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                    } else
                        object.value = options.longs === String ? "0" : 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    if (typeof message.value === "number")
                        object.value = options.longs === String ? String(message.value) : message.value;
                    else
                        object.value = options.longs === String ? $util.Long.prototype.toString.call(message.value) : options.longs === Number ? new $util.LongBits(message.value.low >>> 0, message.value.high >>> 0).toNumber(true) : message.value;
                return object;
            };

            /**
             * Converts this UInt64Value to JSON.
             * @function toJSON
             * @memberof google.protobuf.UInt64Value
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            UInt64Value.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return UInt64Value;
        })();

        protobuf.Int32Value = (function() {

            /**
             * Properties of an Int32Value.
             * @memberof google.protobuf
             * @interface IInt32Value
             * @property {number|null} [value] Int32Value value
             */

            /**
             * Constructs a new Int32Value.
             * @memberof google.protobuf
             * @classdesc Represents an Int32Value.
             * @implements IInt32Value
             * @constructor
             * @param {google.protobuf.IInt32Value=} [properties] Properties to set
             */
            function Int32Value(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * Int32Value value.
             * @member {number} value
             * @memberof google.protobuf.Int32Value
             * @instance
             */
            Int32Value.prototype.value = 0;

            /**
             * Creates a new Int32Value instance using the specified properties.
             * @function create
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {google.protobuf.IInt32Value=} [properties] Properties to set
             * @returns {google.protobuf.Int32Value} Int32Value instance
             */
            Int32Value.create = function create(properties) {
                return new Int32Value(properties);
            };

            /**
             * Encodes the specified Int32Value message. Does not implicitly {@link google.protobuf.Int32Value.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {google.protobuf.IInt32Value} message Int32Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Int32Value.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 0 =*/8).int32(message.value);
                return writer;
            };

            /**
             * Encodes the specified Int32Value message, length delimited. Does not implicitly {@link google.protobuf.Int32Value.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {google.protobuf.IInt32Value} message Int32Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            Int32Value.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes an Int32Value message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.Int32Value} Int32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Int32Value.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.Int32Value();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.int32();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes an Int32Value message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.Int32Value} Int32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            Int32Value.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies an Int32Value message.
             * @function verify
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            Int32Value.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isInteger(message.value))
                        return "value: integer expected";
                return null;
            };

            /**
             * Creates an Int32Value message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.Int32Value} Int32Value
             */
            Int32Value.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.Int32Value)
                    return object;
                let message = new $root.google.protobuf.Int32Value();
                if (object.value != null)
                    message.value = object.value | 0;
                return message;
            };

            /**
             * Creates a plain object from an Int32Value message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.Int32Value
             * @static
             * @param {google.protobuf.Int32Value} message Int32Value
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            Int32Value.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                return object;
            };

            /**
             * Converts this Int32Value to JSON.
             * @function toJSON
             * @memberof google.protobuf.Int32Value
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            Int32Value.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return Int32Value;
        })();

        protobuf.UInt32Value = (function() {

            /**
             * Properties of a UInt32Value.
             * @memberof google.protobuf
             * @interface IUInt32Value
             * @property {number|null} [value] UInt32Value value
             */

            /**
             * Constructs a new UInt32Value.
             * @memberof google.protobuf
             * @classdesc Represents a UInt32Value.
             * @implements IUInt32Value
             * @constructor
             * @param {google.protobuf.IUInt32Value=} [properties] Properties to set
             */
            function UInt32Value(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * UInt32Value value.
             * @member {number} value
             * @memberof google.protobuf.UInt32Value
             * @instance
             */
            UInt32Value.prototype.value = 0;

            /**
             * Creates a new UInt32Value instance using the specified properties.
             * @function create
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {google.protobuf.IUInt32Value=} [properties] Properties to set
             * @returns {google.protobuf.UInt32Value} UInt32Value instance
             */
            UInt32Value.create = function create(properties) {
                return new UInt32Value(properties);
            };

            /**
             * Encodes the specified UInt32Value message. Does not implicitly {@link google.protobuf.UInt32Value.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {google.protobuf.IUInt32Value} message UInt32Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            UInt32Value.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.value);
                return writer;
            };

            /**
             * Encodes the specified UInt32Value message, length delimited. Does not implicitly {@link google.protobuf.UInt32Value.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {google.protobuf.IUInt32Value} message UInt32Value message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            UInt32Value.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a UInt32Value message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.UInt32Value} UInt32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            UInt32Value.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.UInt32Value();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.uint32();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a UInt32Value message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.UInt32Value} UInt32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            UInt32Value.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a UInt32Value message.
             * @function verify
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            UInt32Value.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isInteger(message.value))
                        return "value: integer expected";
                return null;
            };

            /**
             * Creates a UInt32Value message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.UInt32Value} UInt32Value
             */
            UInt32Value.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.UInt32Value)
                    return object;
                let message = new $root.google.protobuf.UInt32Value();
                if (object.value != null)
                    message.value = object.value >>> 0;
                return message;
            };

            /**
             * Creates a plain object from a UInt32Value message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.UInt32Value
             * @static
             * @param {google.protobuf.UInt32Value} message UInt32Value
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            UInt32Value.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = 0;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                return object;
            };

            /**
             * Converts this UInt32Value to JSON.
             * @function toJSON
             * @memberof google.protobuf.UInt32Value
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            UInt32Value.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return UInt32Value;
        })();

        protobuf.BoolValue = (function() {

            /**
             * Properties of a BoolValue.
             * @memberof google.protobuf
             * @interface IBoolValue
             * @property {boolean|null} [value] BoolValue value
             */

            /**
             * Constructs a new BoolValue.
             * @memberof google.protobuf
             * @classdesc Represents a BoolValue.
             * @implements IBoolValue
             * @constructor
             * @param {google.protobuf.IBoolValue=} [properties] Properties to set
             */
            function BoolValue(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * BoolValue value.
             * @member {boolean} value
             * @memberof google.protobuf.BoolValue
             * @instance
             */
            BoolValue.prototype.value = false;

            /**
             * Creates a new BoolValue instance using the specified properties.
             * @function create
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {google.protobuf.IBoolValue=} [properties] Properties to set
             * @returns {google.protobuf.BoolValue} BoolValue instance
             */
            BoolValue.create = function create(properties) {
                return new BoolValue(properties);
            };

            /**
             * Encodes the specified BoolValue message. Does not implicitly {@link google.protobuf.BoolValue.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {google.protobuf.IBoolValue} message BoolValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BoolValue.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 0 =*/8).bool(message.value);
                return writer;
            };

            /**
             * Encodes the specified BoolValue message, length delimited. Does not implicitly {@link google.protobuf.BoolValue.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {google.protobuf.IBoolValue} message BoolValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BoolValue.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a BoolValue message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.BoolValue} BoolValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BoolValue.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.BoolValue();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.bool();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a BoolValue message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.BoolValue} BoolValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BoolValue.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a BoolValue message.
             * @function verify
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            BoolValue.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (typeof message.value !== "boolean")
                        return "value: boolean expected";
                return null;
            };

            /**
             * Creates a BoolValue message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.BoolValue} BoolValue
             */
            BoolValue.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.BoolValue)
                    return object;
                let message = new $root.google.protobuf.BoolValue();
                if (object.value != null)
                    message.value = Boolean(object.value);
                return message;
            };

            /**
             * Creates a plain object from a BoolValue message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.BoolValue
             * @static
             * @param {google.protobuf.BoolValue} message BoolValue
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            BoolValue.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = false;
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                return object;
            };

            /**
             * Converts this BoolValue to JSON.
             * @function toJSON
             * @memberof google.protobuf.BoolValue
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            BoolValue.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return BoolValue;
        })();

        protobuf.StringValue = (function() {

            /**
             * Properties of a StringValue.
             * @memberof google.protobuf
             * @interface IStringValue
             * @property {string|null} [value] StringValue value
             */

            /**
             * Constructs a new StringValue.
             * @memberof google.protobuf
             * @classdesc Represents a StringValue.
             * @implements IStringValue
             * @constructor
             * @param {google.protobuf.IStringValue=} [properties] Properties to set
             */
            function StringValue(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * StringValue value.
             * @member {string} value
             * @memberof google.protobuf.StringValue
             * @instance
             */
            StringValue.prototype.value = "";

            /**
             * Creates a new StringValue instance using the specified properties.
             * @function create
             * @memberof google.protobuf.StringValue
             * @static
             * @param {google.protobuf.IStringValue=} [properties] Properties to set
             * @returns {google.protobuf.StringValue} StringValue instance
             */
            StringValue.create = function create(properties) {
                return new StringValue(properties);
            };

            /**
             * Encodes the specified StringValue message. Does not implicitly {@link google.protobuf.StringValue.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.StringValue
             * @static
             * @param {google.protobuf.IStringValue} message StringValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StringValue.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.value);
                return writer;
            };

            /**
             * Encodes the specified StringValue message, length delimited. Does not implicitly {@link google.protobuf.StringValue.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.StringValue
             * @static
             * @param {google.protobuf.IStringValue} message StringValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            StringValue.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a StringValue message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.StringValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.StringValue} StringValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StringValue.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.StringValue();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.string();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a StringValue message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.StringValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.StringValue} StringValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            StringValue.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a StringValue message.
             * @function verify
             * @memberof google.protobuf.StringValue
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            StringValue.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!$util.isString(message.value))
                        return "value: string expected";
                return null;
            };

            /**
             * Creates a StringValue message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.StringValue
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.StringValue} StringValue
             */
            StringValue.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.StringValue)
                    return object;
                let message = new $root.google.protobuf.StringValue();
                if (object.value != null)
                    message.value = String(object.value);
                return message;
            };

            /**
             * Creates a plain object from a StringValue message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.StringValue
             * @static
             * @param {google.protobuf.StringValue} message StringValue
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            StringValue.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    object.value = "";
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = message.value;
                return object;
            };

            /**
             * Converts this StringValue to JSON.
             * @function toJSON
             * @memberof google.protobuf.StringValue
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            StringValue.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return StringValue;
        })();

        protobuf.BytesValue = (function() {

            /**
             * Properties of a BytesValue.
             * @memberof google.protobuf
             * @interface IBytesValue
             * @property {Uint8Array|null} [value] BytesValue value
             */

            /**
             * Constructs a new BytesValue.
             * @memberof google.protobuf
             * @classdesc Represents a BytesValue.
             * @implements IBytesValue
             * @constructor
             * @param {google.protobuf.IBytesValue=} [properties] Properties to set
             */
            function BytesValue(properties) {
                if (properties)
                    for (let keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                        if (properties[keys[i]] != null)
                            this[keys[i]] = properties[keys[i]];
            }

            /**
             * BytesValue value.
             * @member {Uint8Array} value
             * @memberof google.protobuf.BytesValue
             * @instance
             */
            BytesValue.prototype.value = $util.newBuffer([]);

            /**
             * Creates a new BytesValue instance using the specified properties.
             * @function create
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {google.protobuf.IBytesValue=} [properties] Properties to set
             * @returns {google.protobuf.BytesValue} BytesValue instance
             */
            BytesValue.create = function create(properties) {
                return new BytesValue(properties);
            };

            /**
             * Encodes the specified BytesValue message. Does not implicitly {@link google.protobuf.BytesValue.verify|verify} messages.
             * @function encode
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {google.protobuf.IBytesValue} message BytesValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BytesValue.encode = function encode(message, writer) {
                if (!writer)
                    writer = $Writer.create();
                if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                    writer.uint32(/* id 1, wireType 2 =*/10).bytes(message.value);
                return writer;
            };

            /**
             * Encodes the specified BytesValue message, length delimited. Does not implicitly {@link google.protobuf.BytesValue.verify|verify} messages.
             * @function encodeDelimited
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {google.protobuf.IBytesValue} message BytesValue message or plain object to encode
             * @param {$protobuf.Writer} [writer] Writer to encode to
             * @returns {$protobuf.Writer} Writer
             */
            BytesValue.encodeDelimited = function encodeDelimited(message, writer) {
                return this.encode(message, writer).ldelim();
            };

            /**
             * Decodes a BytesValue message from the specified reader or buffer.
             * @function decode
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @param {number} [length] Message length if known beforehand
             * @returns {google.protobuf.BytesValue} BytesValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BytesValue.decode = function decode(reader, length) {
                if (!(reader instanceof $Reader))
                    reader = $Reader.create(reader);
                let end = length === undefined ? reader.len : reader.pos + length, message = new $root.google.protobuf.BytesValue();
                while (reader.pos < end) {
                    let tag = reader.uint32();
                    switch (tag >>> 3) {
                    case 1:
                        message.value = reader.bytes();
                        break;
                    default:
                        reader.skipType(tag & 7);
                        break;
                    }
                }
                return message;
            };

            /**
             * Decodes a BytesValue message from the specified reader or buffer, length delimited.
             * @function decodeDelimited
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
             * @returns {google.protobuf.BytesValue} BytesValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            BytesValue.decodeDelimited = function decodeDelimited(reader) {
                if (!(reader instanceof $Reader))
                    reader = new $Reader(reader);
                return this.decode(reader, reader.uint32());
            };

            /**
             * Verifies a BytesValue message.
             * @function verify
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {Object.<string,*>} message Plain object to verify
             * @returns {string|null} `null` if valid, otherwise the reason why it is not
             */
            BytesValue.verify = function verify(message) {
                if (typeof message !== "object" || message === null)
                    return "object expected";
                if (message.value != null && message.hasOwnProperty("value"))
                    if (!(message.value && typeof message.value.length === "number" || $util.isString(message.value)))
                        return "value: buffer expected";
                return null;
            };

            /**
             * Creates a BytesValue message from a plain object. Also converts values to their respective internal types.
             * @function fromObject
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {Object.<string,*>} object Plain object
             * @returns {google.protobuf.BytesValue} BytesValue
             */
            BytesValue.fromObject = function fromObject(object) {
                if (object instanceof $root.google.protobuf.BytesValue)
                    return object;
                let message = new $root.google.protobuf.BytesValue();
                if (object.value != null)
                    if (typeof object.value === "string")
                        $util.base64.decode(object.value, message.value = $util.newBuffer($util.base64.length(object.value)), 0);
                    else if (object.value.length)
                        message.value = object.value;
                return message;
            };

            /**
             * Creates a plain object from a BytesValue message. Also converts values to other types if specified.
             * @function toObject
             * @memberof google.protobuf.BytesValue
             * @static
             * @param {google.protobuf.BytesValue} message BytesValue
             * @param {$protobuf.IConversionOptions} [options] Conversion options
             * @returns {Object.<string,*>} Plain object
             */
            BytesValue.toObject = function toObject(message, options) {
                if (!options)
                    options = {};
                let object = {};
                if (options.defaults)
                    if (options.bytes === String)
                        object.value = "";
                    else {
                        object.value = [];
                        if (options.bytes !== Array)
                            object.value = $util.newBuffer(object.value);
                    }
                if (message.value != null && message.hasOwnProperty("value"))
                    object.value = options.bytes === String ? $util.base64.encode(message.value, 0, message.value.length) : options.bytes === Array ? Array.prototype.slice.call(message.value) : message.value;
                return object;
            };

            /**
             * Converts this BytesValue to JSON.
             * @function toJSON
             * @memberof google.protobuf.BytesValue
             * @instance
             * @returns {Object.<string,*>} JSON object
             */
            BytesValue.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };

            return BytesValue;
        })();

        return protobuf;
    })();

    return google;
})();

export { $root as default };
