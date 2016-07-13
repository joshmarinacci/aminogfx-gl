'use strict';

var amino = exports;
var input = require('./aminoinput');
var prims = require('./primitives');

exports.input = input;
exports.primitives = prims;

/**
 * Create properties.
 */
amino.makeProps = function (obj, props) {
    for (var name in props) {
        amino.makeProp(obj, name, props[name]);
    }

    return obj;
};

/**
 * Create property handlers.
 *
 * @param obj object reference.
 * @param name property name.
 * @param val default value.
 */
amino.makeProp = function (obj, name, val) {
    /**
     * Property function.
     *
     * Getter and setter.
     */
    var prop = function (v, nativeCall) {
        if (v != undefined) {
            return prop.set(v, obj, nativeCall);
        } else {
            return prop.get();
        }
    };

    prop.value = val;
    prop.propName = name;
    prop.readonly = false;
    prop.nativeListener = null;
    prop.listeners = [];

    /**
     * Add watch callback.
     *
     * Callback: (value, property, object)
     */
    prop.watch = function (fun) {
        if (!fun) {
            throw new Error('function undefined for property ' + name + ' on object with value ' + val);
        }

        this.listeners.push(fun);

        return this;
    };

    /**
     * Unwatch a registered function.
     */
    prop.unwatch = function (fun) {
        var n = this.listeners.indexOf(fun);

        if (n == -1) {
            throw new Error('function was not registered');
        }

        this.listeners.splice(n, 1);
    };

    /**
     * Getter function.
     */
    prop.get = function () {
        return this.value;
    };

    /**
     * Setter function.
     */
    prop.set = function (v, obj, nativeCall) {
        //check readonly
        if (this.readonly) {
            //ignore any changes
            return obj;
        }

        //check if modified
        if (v === this.value) {
            //debug
            //console.log('not changed: ' + name);

            return obj;
        }

        //update
        this.value = v;

        //native listener
        if (this.nativeListener && !nativeCall) {
            //prevent recursion in case of updates from native side
            this.nativeListener(this.value, this.propId, obj);
        }

        //fire listeners
        for (var i = 0; i < this.listeners.length; i++) {
            this.listeners[i](this.value, this, obj);
        }

        return obj;
    };

    /**
     * Create animation.
     */
    prop.anim = function () {
        //cbx anim
        return amino.getCore().getNative().createPropAnim(obj, name);
    };

    /**
     * Bind to other property.
     *
     * Optional: callback to modify value.
     */
    prop.bindto = function (prop, fun) {
        var set = this;

        prop.listeners.push(function (v) {
            if (fun) {
                set(fun(v));
            } else {
                set(v);
            }
        });

        return this;
    };

    //attach
    obj[name] = prop;
};

//String extension (polyfill for < ES 6)
if (typeof String.prototype.endsWith !== 'function') {
    String.prototype.endsWith = function (suffix) {
        return this.indexOf(suffix, this.length - suffix.length) !== -1;
    };
}

amino.getCore = function () {
    return Core._core;
};

//basic
amino.Group = prims.Group;
amino.Rect = prims.Rect;
amino.Text = prims.Text;
amino.Polygon = prims.Polygon;
amino.ImageView = prims.ImageView;
amino.Circle = prims.Circle;

//extended
amino.PixelView    = prims.PixelView;
amino.RichTextView = prims.RichTextView;
