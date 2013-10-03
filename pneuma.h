#pragma once
#include<string>
#include<vector>
#include<memory>
#include<algorithm>
#include<functional>
#include<array>
#include<regex>
#include<boost\scope_exit.hpp>
template<class Iterator>
struct Pneuma_DebugData
{
	Pneuma_DebugData(const Iterator &begin)
		:max_back(begin)
	{}
	void SetMaxBack(const Iterator&it)
	{
		max_back=std::max(max_back,it);
	}
	const Pneuma_DebugData& operator+=(const Pneuma_DebugData&rhs)
	{
		max_back=std::max(max_back,rhs.max_back);
		return *this;
	}
	Iterator max_back;
};

template<class T,class ITERATOR,class RESULT_TYPE,class BUILD_TYPE> //Buildで返す型
class basic_pneuma{
public:
	typedef ITERATOR Iterator;
	typedef RESULT_TYPE result_type;
	typedef BUILD_TYPE build_type;
	struct CompileError:std::runtime_error
	{
		CompileError(const std::string&e):
			std::runtime_error(e)
		{}
	};
	struct BNF_error:CompileError
	{
		BNF_error(const std::string&e)
			:CompileError(e)
		{}
	};
	struct Ast_error:CompileError
	{
		Ast_error(const std::string&e)
			:CompileError(e)
		{}
	};
protected:
	class Ast;
	typedef std::function<result_type()> Call_Func;
	typedef std::shared_ptr<Call_Func> CallFuncPtr;
	typedef std::shared_ptr<Ast> ast_ptr;
	typedef std::weak_ptr<Ast> ast_weak;
	class Ast :public std::enable_shared_from_this<Ast>
	{
	public:
		friend basic_pneuma;
		Ast(const CallFuncPtr&func)
			:func(func)
		{}
		virtual const T&GetElement(size_t n)const=0;
		virtual size_t element_size()const=0;
		virtual size_t child_size()const=0;
		virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const=0;
		
	private:
		result_type eval(ast_weak &now_ast_ptr)const
		{
			auto temp=now_ast_ptr;
			now_ast_ptr=shared_from_this(); 
			BOOST_SCOPE_EXIT((&now_ast_ptr)(&temp)){
				now_ast_ptr=temp;
			}BOOST_SCOPE_EXIT_END
			if(func)
				return (*func)();
			else
			{
				if(child_size()==0)
					throw BNF_error("DefaultFuncError");
				for(size_t i=0;i<child_size()-1;++i)
					Child(i,now_ast_ptr);
				return Child(child_size()-1,now_ast_ptr);
			}
		};
		CallFuncPtr func;
	};

	ast_weak _now_astptr;
	result_type Child(size_t n)
	{
		if(auto i=_now_astptr.lock())
			return i->Child(n,_now_astptr);
		throw Ast_error("BadChildCall");
	}
	size_t ChildSize()const
	{
		if(auto i=_now_astptr.lock())
			return i->child_size();
		throw Ast_error("BadChildSizeCall");
	}
	const T&GetToken(size_t n)
	{
		if(auto i=_now_astptr.lock())
			return i->GetElement(n);
		throw Ast_error("BadGetTokenCall");
	}
	size_t TokenSize()const
	{
		if(auto i=_now_astptr.lock())
			return i->element_size();
		throw Ast_error("CantTokenSizeGet");
	}

private:
	template<class Data,class Asts>
	static ast_ptr MakeAst(Data&data,Asts&ast,const CallFuncPtr&func)
	{
		class normall_Ast :public Ast
		{
		public:
			normall_Ast(Data&data,Asts&ast,const CallFuncPtr&func)
				:child_data(ast.begin(),ast.end())
				,element_data(data.begin(),data.end())
				,Ast(func)
			{}
			virtual const T&GetElement(size_t n)const{return element_data.at(n);}
			virtual size_t element_size()const{return element_data.size();}
			virtual size_t child_size()const{return child_data.size();}
			virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const
			{
				return child_data.at(n)->eval(now_ast_ptr);
			}
		
		private:
			std::vector<T> element_data;
			std::vector<ast_ptr> child_data;
		};
		return std::make_shared<normall_Ast>(data,ast,func);
	}
	

	static ast_ptr MakeAst(const CallFuncPtr&func)
	{
		class func_Ast :public Ast
		{
		public:
			func_Ast(const CallFuncPtr&func)
				:Ast(func)
			{}
			virtual const T&GetElement(size_t n)const{throw Ast_error("BadAccsessElement");}
			virtual size_t element_size()const{return 0;}
			virtual size_t child_size()const{return 0;}
			virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const{throw Ast_error("BadAccsessElement");	}
		};
		return std::make_shared<func_Ast>(func);
	}
protected:

	//holder(=構文木)が必要とするデータ
	struct holder_data
	{
		holder_data()
			:func(CallFuncPtr(nullptr))
		{}
		std::vector<T> st;
		std::vector<ast_ptr> ast;
		CallFuncPtr func;
		const holder_data&operator+=(const holder_data&rhs)
		{
			if(rhs.func)
				func=rhs.func;
			st.insert(st.end(),rhs.st.begin(),rhs.st.end());
			ast.insert(ast.end(),rhs.ast.begin(),rhs.ast.end());
			return *this;
		}
	};


public:
	typedef Pneuma_DebugData<Iterator> DebugData;
protected:
	class Factor
	{
	public:
		virtual bool operator()(Iterator &it,const Iterator&end,holder_data&,DebugData&)const=0;
	};
	typedef std::shared_ptr<Factor> Factor_Ptr;
	class Symbol;
	

	class Facter_expr
	{
	public:
		Facter_expr(const Factor_Ptr&pt)
		{
			data.push_back(pt);
		}
		Facter_expr(std::vector<Factor_Ptr>&&original)
			:data(std::move(original))
		{}
		
		Facter_expr operator&(const Facter_expr&rhs)const
		{
			std::vector<Factor_Ptr> temp;
			temp.reserve(data.size()+rhs.data.size());
			temp=data;
			temp.insert(temp.end(),rhs.data.begin(),rhs.data.end());
			return std::move(temp);
		}
		inline Facter_expr operator |(const Facter_expr&rhs)const
		{
			class Or:public Factor
			{
			public:
				Or(const Facter_expr&lhs,const Facter_expr&rhs)
					:lhs(lhs),rhs(rhs)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&debug)const
				{
					holder_data h;
					Iterator i=it;
					if((*lhs)(i,end,h,debug))
					{
						holder+=h;
						it=i;
						return true;
					}
					return (*rhs)(it,end,holder,debug);
				}
			private:
				Factor_Ptr lhs,rhs;
			};
			return std::make_shared<Or>(*this,rhs); 
		}
		inline Facter_expr operator *()const
		{
			class Loop:public Factor
			{
			public:
				Loop(const Facter_expr&rhs)
					:rhs(rhs)
				{}

				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					if((*rhs)(it,end,holder,result))
						(*this)(it,end,holder,result);
					return true;

					holder_data h;
					Iterator i=it;
					if((*rhs)(i,end,h,result) && (*this)(i,end,h,result))
					{
						holder+=h;
						it=i;
					}
					return true;
				}
			private:
				Factor_Ptr rhs;
			};
			return std::make_shared<Loop>(*this);
		}
		inline Facter_expr operator +()const
		{
			return *this&*(*this);
		}
		inline Facter_expr operator!()const
		{
			class OK:public Factor
			{
			public:
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					return true;
				}
			private:
			};
			return (*this)|Facter_expr(std::make_shared<OK>());
		}
		operator Factor_Ptr()const
		{
			class wrapper_Facter_expr:public Factor
			{
			public:
				wrapper_Facter_expr(const std::vector<Factor_Ptr>&data)
					:data(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					holder_data h=holder;
					Iterator i=it;
					if( 
						std::all_of(
						data.begin(),data.end(),[&](const Factor_Ptr&factor){
							return (*factor)(i,end,h,result);
						})
					)
						{
							it=i;
							holder=h;
							return true;
						}
						return false;
				}
			private:
				std::vector<Factor_Ptr> data;
			};
			return std::make_shared<wrapper_Facter_expr>(data);
		}
		Factor_Ptr Wrap()const
		{
			return *this;
		}
		const std::vector<Factor_Ptr>&Get()const
		{
			return data;
		}
	private:
		std::vector<Factor_Ptr> data;
	};
	typedef std::vector<Factor_Ptr> HolderData;
	class Symbol
	{
	public:
		Symbol(HolderData&data)
			:data(data)
		{}
		inline void operator=(const Facter_expr&rhs)
		{
			data=rhs.Get();
		}
		inline void operator=(const Symbol&rhs)
		{
			data=static_cast<Facter_expr>(rhs).Get();
		}
		bool operator()(Iterator &it,const Iterator&end,holder_data& hold,DebugData&result)const
		{
			holder_data myhold;
			Iterator i=it;
			if(std::all_of(data.begin(),data.end(),[&](const Factor_Ptr&factor){return (*factor)(i,end,myhold,result);}))
			{
				if(myhold.st.size()==0 && !myhold.func && myhold.ast.size()==1)
				{
					hold.ast.push_back(myhold.ast.at(0));
				}
				else
				{
					hold.ast.push_back(MakeAst(myhold.st,myhold.ast,myhold.func));
				}
				it=i;
				return true;
			}
			return false;
		}
		operator Facter_expr()const
		{
			class ref_Holder:public Factor
			{
			public:
				ref_Holder(HolderData &data)
					:holder(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
				{
					return holder(it,end,hold,result);
				}
			private:
				Symbol holder;
			};
			return Factor_Ptr (new ref_Holder(data));
		}
		inline Facter_expr operator&(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)&rhs;
		}
		inline Facter_expr operator|(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)|rhs;
		}
		inline Facter_expr operator*()const
		{
			return *static_cast<Facter_expr>(*this);
		}
		inline Facter_expr operator!()const
		{
			return !static_cast<Facter_expr>(*this);
		}
		inline Facter_expr operator+()const
		{
			return +static_cast<Facter_expr>(*this);
		}
		inline HolderData&GetHolderData()
		{
			return data;
		}
	private:
		HolderData &data;
	};

	Symbol MakeSymbol()
	{
		auto p=std::make_shared<HolderData>();
		node_data.push_back(p);
		return Symbol(*p);
	}
	void SetStart(const Facter_expr&rhs)
	{
		start=std::make_shared<HolderData>();
		Symbol h(*start);
		h=rhs;
	}
		
private:
	typedef std::shared_ptr<HolderData> HolderDataPtr;
	std::vector<HolderDataPtr> node_data;
	HolderDataPtr start;
	holder_data holder;
public:
	void Parse(const Iterator&begin,const Iterator&end)
	{
		auto it=begin;
		DebugData debug(it);
		bool r=Parse(it,end,debug);
		if(!r || it!=end )
		{
			throw debug;
		}
	}
	bool Parse(Iterator&it,const Iterator&end,DebugData &debug)
	{
		if(!start)throw BNF_error("NotDefinedStart");
		Symbol node(*start);
		return node(it,end,holder,debug);
	}
	virtual build_type Build()=0;
protected:
	result_type eval()
	{
		return (*holder.ast.at(0)).eval(_now_astptr);
	}

	static Facter_expr Set(const Call_Func&func)
	{
		class SetCallFunc:public Factor
		{
		public:
			SetCallFunc(const Call_Func&f)
				:fun(std::make_shared<Call_Func>(f))
			{}
			bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
			{
				holder.func=fun;
				return true;
			}
		private:
			CallFuncPtr fun;
		};
		return std::make_shared<SetCallFunc>(func);
	}

	//普通の終端記号用のテンプレート。funcは条件、pushはトークンを保存するか
	template<class FUNC,bool PUSH>
	class basic_terminal_symbol:public Factor
	{
	public:
		basic_terminal_symbol(const FUNC&f)
			:f(f)
		{}
		bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&debug)const
		{
			if(it!=end && f(*it))
			{
				if(PUSH)
					holder.st.push_back(*it);
				++it;
				return true;
			}
			debug.SetMaxBack(it);
			return false;
		}
	private:
		FUNC f;
	}; 
private:
	class _Match
	{
	public:
		_Match(const T&x)
			:data(x)
		{}
		bool operator()(const T&x)const
		{
			return x==data;
		}
	private:
		T data;
	};
protected:
	static Facter_expr S(const T&code)
	{
		
		typedef basic_terminal_symbol<_Match,false> sym;
		return Factor_Ptr(new sym(_Match(code)));
	}
	static Facter_expr SP(const T&code)
	{
		typedef basic_terminal_symbol<_Match,true> sym;
		return Factor_Ptr(new sym(_Match(code)));
	}

	//pneuma→symbol
	template<class PNEUMA>
	class PneumaWrapper
	{
		typedef std::function<std::shared_ptr<PNEUMA>()> Data;
	public:

		//VS2013来たら本気出す
		//引数ぶち込め
		PneumaWrapper()
			:data([]{return std::make_shared<PNEUMA>();})
		{}
		
		template<class U1>
		PneumaWrapper(U1&&a1)
			:data([]{return std::make_shared<PNEUMA>(a1);})
		{}
		
		template<class U1,class U2>
		PneumaWrapper(U1&&a1,U2&&a2)
			:data([]{return std::make_shared<PNEUMA>(a1,a2);})
		{}

		template<class U1,class U2,class U3>
		PneumaWrapper(U1&&a1,U2&&a2,U3&&a3)
			:data([]{return std::make_shared<PNEUMA>(a1,a2,a3);})
		{}

		template<class U1,class U2,class U3,class U4>
		PneumaWrapper(U1&&a1,U2&&a2,U3&&a3,U4&&a4)
			:data([]{return std::make_shared<PNEUMA>(a1,a2,a3,a4);})
		{}

		Facter_expr operator()()
		{
			class pneuma2symbol:public Factor
			{
			public:
				pneuma2symbol(const Data&f)
					:data(f)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
				{
					auto p=data();
					if(p->Parse(it,end,result))
					{
						Call_Func f=[=]{return p->Build();};
						hold.ast.push_back(MakeAst(std::make_shared<Call_Func>(f)));
						return true;
					}
					return false;
				}
			private:
				Data data;
			}; 
			return std::make_shared<pneuma2symbol>(data);
		}
		template<class TransFunction>
		Facter_expr operator()(const TransFunction &func)
		{
			class pneuma2symbol:public Factor
			{
			public:
				pneuma2symbol(const Data&f,const TransFunction &trans)
					:data(f),trans(trans)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
				{
					auto p=data();
					if(p->Parse(it,end,result))
					{
						Call_Func f=[=]{return trans(p->Build());};
						hold.ast.push_back(MakeAst(std::make_shared<Call_Func>(f)));
						return true;
					}
					return false;
				}
			private:
				Data data;
				TransFunction trans;
			}; 
			return std::make_shared<pneuma2symbol>(data,func);
		}
	private:

		std::function<std::shared_ptr<PNEUMA>()> data;

	};


	//カンマ区切りなどの動きをサポートする。空行を許さない
	//[a][aba][ababa]...[ababab...aba]
	inline Facter_expr Sandwich(const Facter_expr&bread ,const Facter_expr&filling)
	{
		return bread &*(filling&bread);
	};


};

template<class Iterator,class result_type,class BuildType>
class pneuma:public basic_pneuma<std::string,Iterator,result_type,BuildType>
{
private:
	class _Reg
	{
	public:
		_Reg(const std::string&regex)
			:re(regex)
		{}
		bool operator()(const std::string&x)const
		{
			return std::regex_match(x,re);
		}
	private:
		std::regex re;
	};
protected:
	static Facter_expr RegexSymbol(const std::string&code)
	{
		typedef basic_terminal_symbol<_Reg,true> sym;
		return Factor_Ptr(new sym(_Reg(code)));
	}
	static Facter_expr RegexPushSymbol(const std::string&code)
	{
		typedef basic_terminal_symbol<_Reg,true> sym;
		return Factor_Ptr(new sym(_Reg(code)));
	}
};