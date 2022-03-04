const gettextFuncs = new Set([
    '_',
    'N_',
    'C_',
    'NC_',
    'dcgettext',
    'dgettext',
    'dngettext',
    'dpgettext',
    'gettext',
    'ngettext',
    'pgettext',
]);

function dirname(file) {
    const split = file.split('/');
    split.pop();
    return split.join('/');
}

const scriptDir = dirname(import.meta.url);
const root = dirname(scriptDir);

const excludedFiles = new Set();
const foundFiles = new Set()

function addExcludes(filename) {
    const contents = os.file.readFile(filename);
    const lines = contents.split('\n')
        .filter(l => l && !l.startsWith('#'));
   lines.forEach(line => excludedFiles.add(line));
}

addExcludes(`${root}/po/POTFILES.in`);
addExcludes(`${root}/po/POTFILES.skip`);

function walkAst(node, func) {
    func(node);
    nodesToWalk(node).forEach(n => walkAst(n, func));
}

function findGettextCalls(node) {
    switch(node.type) {
    case 'CallExpression':
        if (node.callee.type === 'Identifier' &&
            gettextFuncs.has(node.callee.name))
            throw new Error();
        if (node.callee.type === 'MemberExpression' &&
            node.callee.object.type === 'Identifier' &&
            node.callee.object.name === 'Gettext' &&
            node.callee.property.type === 'Identifier' &&
            gettextFuncs.has(node.callee.property.name))
            throw new Error();
        break;
    }
    return true;
}

function nodesToWalk(node) {
    switch(node.type) {
    case 'ArrayPattern':
    case 'BreakStatement':
    case 'CallSiteObject':  // i.e. strings passed to template
    case 'ContinueStatement':
    case 'DebuggerStatement':
    case 'EmptyStatement':
    case 'Identifier':
    case 'Literal':
    case 'MetaProperty':  // i.e. new.target
    case 'Super':
    case 'ThisExpression':
        return [];
    case 'ArrowFunctionExpression':
    case 'FunctionDeclaration':
    case 'FunctionExpression':
        return [...node.defaults, node.body].filter(n => !!n);
    case 'AssignmentExpression':
    case 'BinaryExpression':
    case 'ComprehensionBlock':
    case 'LogicalExpression':
        return [node.left, node.right];
    case 'ArrayExpression':
    case 'TemplateLiteral':
        return node.elements.filter(n => !!n);
    case 'BlockStatement':
    case 'Program':
        return node.body;
    case 'StaticClassBlock':
        return [node.body];
    case 'ClassField':
        return [node.name, node.init];
    case 'CallExpression':
    case 'NewExpression':
    case 'OptionalCallExpression':
    case 'TaggedTemplate':
        return [node.callee, ...node.arguments];
    case 'CatchClause':
        return [node.body, node.guard].filter(n => !!n);
    case 'ClassExpression':
    case 'ClassStatement':
        return [...node.body, node.superClass].filter(n => !!n);
    case 'ClassMethod':
        return [node.name, node.body];
    case 'ComprehensionExpression':
    case 'GeneratorExpression':
        return [node.body, ...node.blocks, node.filter].filter(n => !!n);
    case 'ComprehensionIf':
        return [node.test];
    case 'ComputedName':
        return [node.name];
    case 'ConditionalExpression':
    case 'IfStatement':
        return [node.test, node.consequent, node.alternate].filter(n => !!n);
    case 'DoWhileStatement':
    case 'WhileStatement':
        return [node.body, node.test];
    case 'ExportDeclaration':
        return [node.declaration, node.source].filter(n => !!n);
    case 'ImportDeclaration':
        return [...node.specifiers, node.source];
    case 'LetStatement':
        return [...node.head, node.body];
    case 'ExpressionStatement':
        return [node.expression];
    case 'ForInStatement':
    case 'ForOfStatement':
        return [node.body, node.left, node.right];
    case 'ForStatement':
        return [node.init, node.test, node.update, node.body].filter(n => !!n);
    case 'LabeledStatement':
        return [node.body];
    case 'MemberExpression':
        return [node.object, node.property];
    case 'ObjectExpression':
    case 'ObjectPattern':
        return node.properties;
    case 'OptionalExpression':
        return [node.expression];
    case 'OptionalMemberExpression':
        return [node.object, node.property];
    case 'Property':
    case 'PrototypeMutation':
        return [node.value];
    case 'ReturnStatement':
    case 'ThrowStatement':
    case 'UnaryExpression':
    case 'UpdateExpression':
    case 'YieldExpression':
        return node.argument ? [node.argument] : [];
    case 'SequenceExpression':
        return node.expressions;
    case 'SpreadExpression':
        return [node.expression];
    case 'SwitchCase':
        return [node.test, ...node.consequent].filter(n => !!n);
    case 'SwitchStatement':
        return [node.discriminant, ...node.cases];
    case 'TryStatement':
        return [node.block, node.handler, node.finalizer].filter(n => !!n);
    case 'VariableDeclaration':
        return node.declarations;
    case 'VariableDeclarator':
        return node.init ? [node.init] : [];
    case 'WithStatement':
        return [node.object, node.body];
    default:
        print(`Ignoring ${node.type}, you should probably fix this in the script`);
    }
}

function walkDir(dir) {
    os.file.listDir(dir).forEach(child => {
        if (child.startsWith('.'))
            return;

        const path = os.path.join(dir, child);
        const relativePath = path.replace(`${root}/`, '');
        if (excludedFiles.has(relativePath))
            return;

        if (!child.endsWith('.js')) {
            try {
                walkDir(path);
            } catch (e) {
                // not a directory
            }
            return;
        }

        try {
            const script = os.file.readFile(path);
            const ast = Reflect.parse(script);
            walkAst(ast, findGettextCalls);
        } catch (e) {
            foundFiles.add(path);
        }
    });
}

walkDir(root);

if (foundFiles.size === 0)
    quit(0);

print('The following files are missing from po/POTFILES.in:')
foundFiles.forEach(f => print(`  ${f}`));
quit(1);
