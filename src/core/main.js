'use strict';

var amino = require('./src/amino.js');
var input = require('./src/aminoinput.js');

var DEBUG = false;

exports.input = input;

/**
 * Core object.
 */
var Core = function () {
    /**
     * Find a node at a certain position.
     */
    this.findNodesAtXY = function (pt) {
        return findNodesAtXY_helper(this.root, pt, null, '');
    };

    /**
     * Find a node at a certain position with a filter callback.
     */
    this.findNodesAtXYFiltered = function (pt, filter) {
        return findNodesAtXY_helper(this.root, pt, filter, '');
    };

    function findNodesAtXY_helper(root, pt, filter, tab) {
        //verify
        if (!root) {
            return [];
        }

        if (!root.visible()) {
            return [];
        }

        //console.log(tab + "   xy",pt.x,pt.y, root.id());

        var tpt = pt.minus(root.x(), root.y()).divide(root.sx(), root.sy());

        //handle children first, then the parent/root
        var res = [];

        if (filter) {
            if (!filter(root)) {
                return res;
            }
        }

        if (root.children && root.children.length && root.children.length > 0) {
            for (var i = root.children.length - 1; i >= 0; i--) {
                var node = root.children[i];
                var found = findNodesAtXY_helper(node, tpt, filter, tab + '  ');

                res = res.concat(found);
            }
        }

        if (root.contains && root.contains(tpt)) {
            res = res.concat([root]);
        }

        return res;
    }

    /**
     * Find a node at a certain position.
     */
    this.findNodeAtXY = function (x, y) {
        return this.findNodeAtXY_helper(this.root, x, y, '');
    };

    this.findNodeAtXY_helper = function (root, x, y, tab) {
        if (!root) {
            return null;
        }

        if (!root.visible()) {
            return null;
        }

        var tx = x - root.x();
        var ty = y - root.y();

        tx /= root.sx();
        ty /= root.sy();

        //console.log(tab + "   xy="+tx+","+ty);

        if (root.cliprect && root.cliprect()) {
            if (tx < 0) {
                return false;
            }

            if (tx > root.w()) {
                return false;
            }

            if (ty < 0) {
                return false;
            }

            if (ty > root.h()) {
                return false;
            }
        }

        if (root.children) {
            //console.log(tab+"children = ",root.children.length);

            for (var i = root.children.length - 1; i >= 0; i--) {
                var node = root.children[i];
                var found = this.findNodeAtXY_helper(node, tx, ty, tab+ '  ');

                if (found) {
                	return found;
            	}
            }
        }

        //console.log(tab+"contains " + tx+' '+ty);

        if (root.contains && root.contains(tx, ty)) {
            //console.log(tab,"inside!",root.getId());

           return root;
        }

        return null;
    };

    function calcGlobalToLocalTransform(node) {
        if (node.parent) {
            var trans = calcGlobalToLocalTransform(node.parent);

            if (node.getScalex() != 1) {
                trans.x /= node.sx();
                trans.y /= node.sy();
            }

            trans.x -= node.x();
            trans.y -= node.y();

            return trans;
        }

        return {
            x: -node.x(),
            y: -node.y()
        };
    }

    this.globalToLocal = function (pt, node) {
        return this.globalToLocal_helper(pt,node);
    };

    this.globalToLocal_helper = function (pt, node) {
    	if (node.parent) {
    		pt =  this.globalToLocal_helper(pt,node.parent);
    	}

        return exports.input.makePoint(
            (pt.x - node.x()) / node.sx(),
            (pt.y - node.y()) / node.sy()
        );
    };

    this.localToGlobal = function (pt, node) {
        pt = {
            x: pt.x + node.x() * node.sx(),
            y: pt.y + node.y() * node.sx()
        };

        if (node.parent) {
            return this.localToGlobal(pt, node.parent);
        } else {
            return pt;
        }
    };

    this.on = function (name, target, listener) {
        exports.input.on(name, target, listener);
    };
};

exports.PixelView    = amino.PixelView;
exports.RichTextView = amino.RichTextView;