#include "Presenter.h"

#include "ifc/File.h"

#include "ifc/Chart.h"
#include "ifc/Expression.h"
#include "ifc/Declaration.h"
#include "ifc/Type.h"

void Presenter::present(ifc::NameIndex name) const
{
    using enum ifc::NameSort;

    switch (const auto kind = name.sort())
    {
    case Identifier:
        out_ << file_.get_string(ifc::TextOffset{name.index});
        break;
    case Operator:
        const auto operator_function_name = file_.operator_names()[name];
        out_ << "operator" << file_.get_string(operator_function_name.encoded);
        break;
    default:
        out_ << "Unsupported NameSort '" << static_cast<int>(kind) << "'";
    }
}

void Presenter::present(ifc::FunctionType const& function_type) const
{
    present(function_type.target);
    out_ << "(";
    present(function_type.source);
    out_ << ")";
}

void Presenter::present(ifc::FundamentalType const& type) const
{
    switch (type.sign)
    {
    case ifc::TypeSign::Plain:
        break;
    case ifc::TypeSign::Signed:
        out_ << "signed ";
        break;
    case ifc::TypeSign::Unsigned:
        out_ << "unsigned ";
        break;
    }

    switch (type.precision)
    {
    case ifc::TypePrecision::Default:
        break;
    case ifc::TypePrecision::Short:
        assert(type.basis == ifc::TypeBasis::Int);
        out_ << "short";
        return;
    case ifc::TypePrecision::Long:
        out_ << "long ";
        break;
    case ifc::TypePrecision::Bit64:
        assert(type.basis == ifc::TypeBasis::Int);
        out_ << "long long";
        return;
    case ifc::TypePrecision::Bit8:
    case ifc::TypePrecision::Bit16:
    case ifc::TypePrecision::Bit32:
    case ifc::TypePrecision::Bit128:
        out_ << "Unsupported Bitness '" << static_cast<int>(type.precision) << "' ";
        break;
    }

    using enum ifc::TypeBasis;
    switch (type.basis)
    {
    case Void:
        out_ << "void";
        break;
    case Bool:
        out_ << "bool";
        break;
    case Char:
        out_ << "char";
        break;
    case Wchar_t:
        out_ << "wchar_t";
        break;
    case Int:
        out_ << "int";
        break;
    case Float:
        out_ << "float";
        break;
    case Double:
        out_ << "double";
        break;
    default:
        out_ << "fundamental type {" << static_cast<int>(type.basis) << "}";
    }
}

template<typename T, typename Index>
void Presenter::present_heap_slice(ifc::Partition<T, Index> heap, ifc::Sequence seq) const
{
    present_range(heap.slice(seq), ", ");
}

template<typename Range>
void Presenter::present_range(Range range, std::string_view separator) const
{
    bool first = true;
    for (auto element : range)
    {
        if (first)
            first = false;
        else
            out_ << separator;
        present(element);
    }
}

void Presenter::present(ifc::TupleType tuple) const
{
    present_heap_slice(file_.type_heap(), tuple);
}

void Presenter::present(ifc::LvalueReference ref) const
{
    present(ref.referee);
    out_ << "&";
}

void Presenter::present(ifc::RvalueReference ref) const
{
    present(ref.referee);
    out_ << "&&";
}

void Presenter::present(ifc::Qualifiers quals) const
{
    using enum ifc::Qualifiers;

    if (has_qualifier(quals, Const))
        out_ << "const ";
    if (has_qualifier(quals, Volatile))
        out_ << "volatile ";
    if (has_qualifier(quals, Restrict))
        out_ << "__restrict ";
}

void Presenter::present(ifc::QualifiedType qualType) const
{
    present(qualType.qualifiers);
    present(qualType.unqualified);
}

void Presenter::present(ifc::DeclReference decl_ref) const
{
    ifc::File const & imported_module = file_.get_imported_module(decl_ref.unit);
    Presenter(imported_module, out_).present_refered_declaration(decl_ref.local_index);
}

void Presenter::present(ifc::NamedDecl const& decl) const
{
    using enum ifc::DeclSort;

    switch (auto const kind = decl.resolution.sort())
    {
    case Reference:
        present(file_.decl_references()[decl.resolution]);
        break;
    default:
        out_ << "Declaration of unsupported kind '" << static_cast<int>(kind) << "'";
    }
}

void Presenter::present(ifc::TupleExpression const& tuple) const
{
    present_heap_slice(file_.expr_heap(), tuple.seq);
}

void Presenter::present(ifc::ExprIndex expr) const
{
    using enum ifc::ExprSort;
    switch (auto const expr_kind = expr.sort())
    {
    case Type:
        present(file_.type_expressions()[expr].denotation);
        break;
    case NamedDecl:
        present(file_.decl_expressions()[expr]);
        break;
    case Tuple:
        present(file_.tuple_expressions()[expr]);
        break;
    default:
        out_ << "Unsupported ExprSort'" << static_cast<int>(expr_kind) << "'";
    }
}

void Presenter::present(ifc::ChartIndex chart) const
{
    switch (chart.sort())
    {
    case ifc::ChartSort::None:
        out_ << "template<> ";
        break;
    case ifc::ChartSort::Unilevel:
        {
            auto uni_chart = file_.unilevel_charts()[chart];
            out_ << "template<";
            bool first = true;
            for (auto param : file_.parameters().slice(uni_chart))
            {
                if (first)
                    first = false;
                else
                    out_ << ", ";
                switch (param.sort)
                {
                case ifc::ParameterSort::Object:
                    assert(false && "function parameter is unexpected here");
                    break;
                case ifc::ParameterSort::Type:
                    out_ << "typename";
                    break;
                case ifc::ParameterSort::NonType:
                    present(param.type);
                    break;
                case ifc::ParameterSort::Template:
                    assert(param.type.sort() == ifc::TypeSort::Forall);
                    auto forall_type = file_.forall_types()[param.type];
                    assert(forall_type.chart.sort() == ifc::ChartSort::Unilevel);
                    present(forall_type.chart);
                    assert(forall_type.subject.sort() == ifc::TypeSort::Fundamental);
                    assert(file_.fundamental_types()[forall_type.subject].basis == ifc::TypeBasis::Typename);
                    out_ << "typename";
                    break;
                }
                if (param.pack)
                    out_ << "...";
                if (!is_null(param.name))
                    out_ << " " << file_.get_string(param.name);
            }
            out_ << "> ";
        }
        break;
    case ifc::ChartSort::Multilevel:
        out_ << "Chart.Multilevel presentation is unsupported ";
        break;
    }
}

void Presenter::present(ifc::TemplateId const& template_id) const
{
    present(template_id.primary);
    out_ << '<';
    present(template_id.arguments);
    out_ << '>';
}

void Presenter::present(ifc::SyntacticType type) const
{
    using enum ifc::ExprSort;
    switch (auto const expr_kind = type.expr.sort())
    {
    case TemplateId:
        present(file_.template_ids()[type.expr]);
        break;
    default:
        out_ << "Syntactic Type of unsupported expression kind '" << static_cast<int>(expr_kind) << "'";
    }
}

void Presenter::present(ifc::ExpansionType type) const
{
    present(type.pack);
    out_ << "...";
}

void Presenter::present(ifc::TypeIndex type) const
{
    using enum ifc::TypeSort;
    switch (const auto kind = type.sort())
    {
    case Fundamental:
        present(file_.fundamental_types()[type]);
        break;
    case Designated:
        present_refered_declaration(file_.designated_types()[type].decl);
        break;
    case Syntactic:
        present(file_.syntactic_types()[type]);
        break;
    case Expansion:
        present(file_.expansion_types()[type]);
        break;
    case LvalueReference:
        present(file_.lvalue_references()[type]);
        break;
    case RvalueReference:
        present(file_.rvalue_references()[type]);
        break;
    case Function:
        present(file_.function_types()[type]);
        break;
    case Qualified:
        present(file_.qualified_types()[type]);
        break;
    case Tuple:
        present(file_.tuple_types()[type]);
        break;
    default:
        out_ << "Unsupported TypeSort '" << static_cast<int>(kind) << "'";
    }
}

void Presenter::present_refered_declaration(ifc::DeclIndex decl) const
{
    using enum ifc::DeclSort;

    switch (const auto kind = decl.sort())
    {
    case Parameter:
        {
            ifc::ParameterDeclaration const & param = file_.parameters()[decl];
            out_ << file_.get_string(param.name);
        }
        break;
    case Scope:
        {
            ifc::ScopeDeclaration const & scope = file_.scope_declarations()[decl];
            present(scope.name);
        }
        break;
    case Template:
        {
            ifc::TemplateDeclaration const & template_declaration = file_.template_declarations()[decl];
            present(template_declaration.name);
        }
        break;
    case Function:
        {
            ifc::FunctionDeclaration const & function = file_.functions()[decl];
            present(function.name);
        }
        break;
    case Reference:
        present(file_.decl_references()[decl]);
        break;
    default:
        out_ << "Unsupported DeclSort '" << static_cast<int>(kind) << "'";
    }
}

void Presenter::present_scope_members(ifc::ScopeDescriptor scope) const
{
    present_range(file_.declarations().slice(scope), "\n");
}

void Presenter::present(ifc::ScopeDeclaration const& scope) const
{
    const auto type = scope.type;
    assert(type.sort() == ifc::TypeSort::Fundamental);
    switch (const auto scope_kind = file_.fundamental_types()[type].basis)
    {
        using enum ifc::TypeBasis;
    case Class:
        out_ << "Class";
        break;
    case Struct:
        out_ << "Struct";
        break;
    case Union:
        out_ << "Union";
        break;
    case Namespace:
        out_ << "Namespace";
        break;
    case Interface:
        out_ << "__interface";
        break;
    default:
        out_ << "Unknown Scope '" << static_cast<int>(scope_kind) << "'";
    }
    out_ << " '";
    present(scope.name);
    out_ << "'";
    if (auto const def = scope.initializer; is_null(def))
    {
        out_ << ": incomplete";
    }
    else
    {
        out_ << " {\n";
        indent_ += 2;
        present_scope_members(file_.scope_descriptors()[def]);
        indent_ -= 2;

        insert_indent();
        out_ << "}";
    }
    out_ << "\n";
}

void Presenter::insert_indent() const
{
    for (size_t i = 0; i != indent_; ++i)
        out_ << ' ';
}

void Presenter::present(ifc::DeclIndex decl) const
{
    using enum ifc::DeclSort;

    insert_indent();

    switch (const auto kind = decl.sort())
    {
    case VendorExtension:
        out_ << "Vendor Extension\n";
        break;
    case Variable:
        {
            ifc::VariableDeclaration const & variable = file_.variables()[decl];
            out_ << "Variable '";
            present(variable.name);
            out_ << "', type: ";
            present(variable.type);
            out_ << "\n";
        }
        break;
    case Scope:
        present(file_.scope_declarations()[decl]);
        break;
    case Enumeration:
        {
            auto const & enumeration = file_.enumerations()[decl];
            out_ << "Enumeration '" << file_.get_string(enumeration.name) << "'\n";
        }
        break;
    case Alias:
        {
            auto const & alias = file_.alias_declarations()[decl];
            out_ << "Alias '" << file_.get_string(alias.name) << "'\n";
            break;
        }
    case Template:
        {
            auto const & template_declaration = file_.template_declarations()[decl];
            present(template_declaration.chart);
            out_ << "\n";
            present(template_declaration.entity.decl);
        }
        break;
    case Function:
        {
            ifc::FunctionDeclaration const & function = file_.functions()[decl];
            out_ << "Function '";
            present(function.name);
            out_ << "', type: ";
            present(function.type);
            out_ << "\n";
        }
        break;
    case UsingDeclaration:
        {
            auto const & using_declaration = file_.using_declarations()[decl];
            out_ << "Using '";
            present_refered_declaration(using_declaration.resolution);
            out_ << "'\n";
        }
        break;
    default:
        out_ << "Unsupported DeclSort '" << static_cast<int>(kind) << "'\n";
    }
}

void Presenter::present(ifc::Declaration decl) const
{
    present(decl.index);
}
