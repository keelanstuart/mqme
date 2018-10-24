/*
	mqme Library Source File

	Copyright © 2009-2018, Keelan Stuart. All rights reserved.

	mqme (pronounced "make me") is a Windows-only C++ API and library that facilitates easy
	distribution of network	packets	with multiple connection end-points. One-to-many is just
	as easy as one-to-one. Handling different types of incoming data is as simple as writing
	a callback that	recognizes a four character code (the 'CODE' form is the easiest way
	to use it).

	mqme was written as a response to the (IMO) confusing popularity
	of RabbitMQ, ActiveMQ, ZeroMQ, etc. RabbitMQ is written in Erlang and requires
	multiple support installations and configuration files to function -- which, I think,
	is bad for commercial products. ActiveMQ, to my knowledge, is similar. ZeroMQ forces
	the user to conform to transactional patterns that are not conducive to parallel
	processing of requests and does not allow comprehensive, complex, or numerous
	subscriptions. In essence, it was my opinion that none of those packages was
	"good enough" for me, making me write my own.

	mqme is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	mqme is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	See <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

#include <mqme.h>
#include <algorithm>

using namespace mqme;

class CProperty : public IProperty
{
public:
	PROPERTY_TYPE m_Type;
	PROPERTY_ASPECT m_Aspect;
	tstring m_sName;
	FOURCHARCODE m_ID;

	union
	{
		TCHAR *m_s;
		INT64 m_i;
		double m_f;
		GUID m_g;
	};

	CProperty()
	{
		m_Type = PT_NONE;
		m_Aspect = PA_GENERIC;
		m_s = nullptr;
	}

	// call release()!
	virtual ~CProperty()
	{
	}

	virtual const TCHAR *GetName()
	{
		return m_sName.c_str();
	}

	virtual void SetName(const TCHAR *name)
	{
		m_sName = name;
	}

	virtual FOURCHARCODE GetID()
	{
		return m_ID;
	}

	virtual void SetID(FOURCHARCODE id)
	{
		m_ID = id;
	}

	virtual void Reset()
	{
		if (m_Type == PT_STRING)
		{
			if (m_s)
			{
				free(m_s);
				m_s = NULL;
			}
		}

		m_Type = PT_NONE;
		m_Aspect = PA_GENERIC;
	}


	virtual void Release()
	{
		Reset();

		delete this;
	}

	virtual PROPERTY_TYPE GetType()
	{
		return m_Type;
	}

	virtual bool ConvertTo(PROPERTY_TYPE newtype)
	{
		if (newtype == m_Type)
			return true;

		switch (newtype)
		{
			case PT_INT:
			{
				INT64 i;
				SetInt(AsInt(&i));
				break;
			}

			case PT_FLOAT:
			{
				double f;
				SetFloat(AsFloat(&f));
				break;
			}

			case PT_STRING:
			{
				int32_t bufsz = -1;

				switch (m_Type)
				{
					case PT_STRING:
						bufsz = _sctprintf(_T("%s"), m_s);
						break;

					case PT_INT:
						bufsz = _sctprintf(_T("%I64d"), m_i);
						break;

					case PT_FLOAT:
						bufsz = _sctprintf(_T("%f"), m_f);
						break;

					case PT_GUID:
						bufsz = _sctprintf(_T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), m_g.Data1, m_g.Data2, m_g.Data3,
							m_g.Data4[0], m_g.Data4[1], m_g.Data4[2], m_g.Data4[3], m_g.Data4[4], m_g.Data4[5], m_g.Data4[6], m_g.Data4[7]);
						break;
				}

				if (bufsz >= 0)
				{
					bufsz += sizeof(TCHAR);
					bufsz *= sizeof(TCHAR);
					TCHAR *buf = (TCHAR *)malloc(bufsz);
					AsString(buf, bufsz);

					if ((m_Type == PT_STRING) && m_s)
						free(m_s);

					m_s = buf;
				}
				else
				{
					SetString(_T(""));
					return false;
				}

				break;
			}

			case PT_GUID:
			{
				GUID g;
				SetGUID(AsGUID(&g));
				break;
			}
		}

		m_Type = newtype;

		return true;
	}

	virtual PROPERTY_ASPECT GetAspect()
	{
		return m_Aspect;
	}

	virtual void SetAspect(PROPERTY_ASPECT aspect)
	{
		m_Aspect = aspect;
	}

	virtual void SetInt(INT64 val)
	{
		Reset();

		m_Type = PT_INT;
		m_i = val;
	}

	virtual void SetFloat(double val)
	{
		Reset();

		m_Type = PT_FLOAT;
		m_f = val;
	}

	virtual void SetString(const TCHAR *val)
	{
		Reset();

		m_Type = PT_STRING;
		if (val)
		{
			m_s = _tcsdup(val);
		}
	}

	virtual void SetGUID(GUID val)
	{
		Reset();

		m_Type = PT_GUID;
		m_g = val;
	}

	virtual void SetFromProperty(IProperty *pprop)
	{
		if (!pprop)
		{
			Reset();
			return;
		}

		switch (pprop->GetType())
		{
			case PT_STRING:
				SetString(pprop->AsString());
				break;

			case PT_INT:
				SetInt(pprop->AsInt());
				break;

			case PT_FLOAT:
				SetFloat(pprop->AsFloat());
				break;

			case PT_GUID:
				SetGUID(pprop->AsGUID());
				break;
		}
	}

	virtual INT64 AsInt(INT64 *ret)
	{
		INT64 retval;
		if (!ret)
			ret = &retval;

		switch (m_Type)
		{
			case PT_STRING:
				*ret = m_s ? (INT64)_tstoi64(m_s) : 0;
				break;

			case PT_INT:
				*ret = m_i;
				break;

			case PT_FLOAT:
				*ret = (INT64)(m_f);
				break;

			case PT_GUID:
				*ret = 0;
				break;
		}

		return *ret;
	}

	virtual double AsFloat(double *ret)
	{
		double retval;
		if (!ret)
			ret = &retval;

		switch (m_Type)
		{
			case PT_STRING:
				*ret = (double)_tstof(m_s);
				break;

			case PT_INT:
				*ret = (double)m_i;
				break;

			case PT_FLOAT:
				*ret = m_f;
				break;

			case PT_GUID:
				*ret = 0.0;
				break;
		}

		return *ret;
	}

	virtual const TCHAR *AsString(TCHAR *ret, int32_t retsize)
	{
		if (m_Type == PT_STRING)
		{
			if (!ret || (retsize == 0))
				return m_s;
		}

		if (retsize > 0)
		{
			switch (m_Type)
			{
				case PT_STRING:
					_tcsncpy(ret, m_s, retsize);
					break;

				case PT_INT:
					_sntprintf(ret, retsize, _T("%I64d"), m_i);
					break;

				case PT_FLOAT:
					_sntprintf(ret, retsize, _T("%f"), m_f);
					break;

				case PT_GUID:
					_sntprintf(ret, retsize, _T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), m_g.Data1, m_g.Data2, m_g.Data3,
						m_g.Data4[0], m_g.Data4[1], m_g.Data4[2], m_g.Data4[3], m_g.Data4[4], m_g.Data4[5], m_g.Data4[6], m_g.Data4[7]);
					break;
			}
		}

		return ret;
	}

	virtual GUID AsGUID(GUID *ret)
	{
		GUID retval;

		if (!ret)
			ret = &retval;

		memset(ret, 0, sizeof(GUID));

		switch (m_Type)
		{
			case PT_STRING:
			{
				int d[11];
				_sntscanf(m_s, _tcslen(m_s) * sizeof(TCHAR), _T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), &d[0], &d[1], &d[2],
					&d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);

				ret->Data1 = d[0];
				ret->Data2 = d[1];
				ret->Data3 = d[2];
				ret->Data4[0] = d[3];
				ret->Data4[1] = d[4];
				ret->Data4[2] = d[5];
				ret->Data4[3] = d[6];
				ret->Data4[4] = d[7];
				ret->Data4[5] = d[8];
				ret->Data4[6] = d[9];
				ret->Data4[7] = d[10];
				break;
			}

			case PT_INT:
				break;

			case PT_FLOAT:
				break;

			case PT_GUID:
				*ret = m_g;
				break;
		}

		return *ret;
	}

	virtual bool Serialize(SERIALIZE_MODE mode, BYTE *buf, size_t bufsize, size_t *amountused = NULL)
	{
		if (!m_Type || (m_Type >= PT_NUMTYPES))
			return false;

		size_t sz = sizeof(BYTE) /*serialization type*/ + sizeof(FOURCHARCODE) /*id*/ + sizeof(BYTE) /*PROPERTY_TYPE*/;

		if (mode >= SM_BIN_TERSE)
			sz += sizeof(BYTE); /*PROPERTY_ASPECT*/

		if (mode == SM_BIN_VERBOSE)
			sz += (m_sName.length() + 1) * sizeof(TCHAR);

		switch (m_Type)
		{
			case PT_STRING:
				sz += (_tcslen(m_s) + 1) * sizeof(TCHAR);
				break;

			case PT_INT:
				sz += sizeof(INT64);
				break;

			case PT_FLOAT:
				sz += sizeof(double);
				break;

			case PT_GUID:
				sz += sizeof(GUID);
				break;
		}

		if (amountused)
			*amountused = sz;

		if (buf && (bufsize < sz))
			return false;
		else if (!buf)
			return true;

		*buf = BYTE(mode);
		buf += sizeof(BYTE);

		*((FOURCHARCODE *)buf) = m_ID;
		buf += sizeof(FOURCHARCODE);

		*buf = BYTE(m_Type);
		buf += sizeof(BYTE);

		if (mode >= SM_BIN_TERSE)
		{
			*buf = BYTE(m_Aspect);
			buf += sizeof(BYTE);
		}

		if (mode == SM_BIN_VERBOSE)
		{
			memcpy(buf, m_sName.c_str(), sizeof(TCHAR) * (m_sName.length() + 1));
			buf += sizeof(TCHAR) * (m_sName.length() + 1);
		}

		switch (m_Type)
		{
			case PT_STRING:
			{
				size_t bs = sizeof(TCHAR) * (_tcslen(m_s) + 1);
				memcpy(buf, m_sName.c_str(), bs);
				buf += bs;
				break;
			}

			case PT_INT:
				*((INT64 *)buf) = m_i;
				buf += sizeof(INT64);
				break;

			case PT_FLOAT:
				*((double *)buf) = m_f;
				buf += sizeof(double);
				break;

			case PT_GUID:
				*((GUID *)buf) = m_g;
				buf += sizeof(GUID);
				break;
		}

		return true;
	}

	virtual bool Deserialize(BYTE *buf, size_t bufsize, size_t *bytesconsumed)
	{
		Reset();

		BYTE *origbuf = buf;
		if (!buf)
			return false;

		SERIALIZE_MODE mode = SERIALIZE_MODE(*buf);
		if (mode > SM_BIN_VERBOSE)
			return false;
		buf += sizeof(BYTE);

		m_ID = *((FOURCHARCODE *)buf);
		buf += sizeof(FOURCHARCODE);

		m_Type = PROPERTY_TYPE(*buf);
		if (!m_Type || (m_Type >= PT_NUMTYPES))
			return false;
		buf += sizeof(BYTE);

		if (mode >= SM_BIN_TERSE)
		{
			m_Aspect = PROPERTY_ASPECT(*buf);
			if (m_Aspect >= PA_NUMASPECTS)
				return false;
			buf += sizeof(BYTE);
		}

		if (mode == SM_BIN_VERBOSE)
		{
			m_sName = (TCHAR *)buf;
			buf += sizeof(TCHAR) * (m_sName.length() + 1);
		}

		switch (m_Type)
		{
			case PT_STRING:
			{
				m_s = _tcsdup((TCHAR *)buf);
				buf += sizeof(TCHAR) * (_tcslen(m_s) + 1);
				break;
			}

			case PT_INT:
				m_i = *((INT64 *)buf);
				buf += sizeof(INT64);
				break;

			case PT_FLOAT:
				m_f = *((double *)buf);
				buf += sizeof(double);
				break;

			case PT_GUID:
				m_g = *((GUID *)buf);
				buf += sizeof(GUID);
				break;
		}

		if (bytesconsumed)
			*bytesconsumed = (buf - origbuf);

		return (size_t(buf - origbuf) <= bufsize) ? true : false;
	}
};


class CPropertySet : public IPropertySet
{
protected:
	typedef ::std::deque<IProperty *> TPropertyArray;
	TPropertyArray m_Props;

	typedef ::std::map<FOURCHARCODE, IProperty *> TPropertyMap;
	typedef ::std::pair<FOURCHARCODE, IProperty *> TPropertyMapPair;
	TPropertyMap m_mapProps;

public:

	CPropertySet()
	{
	}

	virtual ~CPropertySet()
	{
	}

	virtual void Release()
	{
		DeleteAll();

		delete this;
	}

	virtual IProperty *CreateProperty(const TCHAR *propname, FOURCHARCODE propid)
	{
		TPropertyMap::const_iterator pi = m_mapProps.find(propid);
		if ((pi != m_mapProps.end()) && pi->second)
			return pi->second;

		CProperty *pprop = new CProperty();
		if (pprop)
		{
			pprop->SetName(propname ? propname : _T(""));
			pprop->SetID(propid);

			AddProperty(pprop);
		}

		return pprop;
	}

	// Adds a new property to this property set
	virtual void AddProperty(IProperty *pprop)
	{
		if (!pprop)
			return;

		uint32_t propid = pprop->GetID();
		m_mapProps.insert(TPropertyMapPair(propid, pprop));
		m_Props.insert(m_Props.end(), pprop);
	}

	virtual void DeleteProperty(size_t idx)
	{
		if (idx >= m_Props.size())
			return;

		IProperty *pprop = m_Props[idx];
		if (pprop)
		{
			TPropertyArray::iterator pia = m_Props.begin();
			pia += idx;
			m_Props.erase(pia);

			TPropertyMap::iterator pim = m_mapProps.find(pprop->GetID());
			m_mapProps.erase(pim);

			pprop->Release();
		}
	}

	virtual void DeletePropertyById(FOURCHARCODE propid)
	{
		TPropertyMap::iterator j = m_mapProps.find(propid);
		if (j != m_mapProps.end())
			m_mapProps.erase(j);

		TPropertyArray::const_iterator e = m_Props.end();
		for (TPropertyArray::iterator i = m_Props.begin(); i != e; i++)
		{
			IProperty *pprop = *i;

			if (pprop->GetID() == propid)
			{
				m_Props.erase(i);

				pprop->Release();
				break;
			}
		}
	}


	virtual void DeletePropertyByName(const TCHAR *propname)
	{
		TPropertyArray::const_iterator e = m_Props.end();
		for (TPropertyArray::iterator i = m_Props.begin(); i != e; i++)
		{
			IProperty *pprop = *i;

			if (!_tcsicmp(pprop->GetName(), propname))
			{
				TPropertyMap::iterator j = m_mapProps.find(pprop->GetID());
				if (j != m_mapProps.end())
					m_mapProps.erase(j);

				m_Props.erase(i);

				pprop->Release();
				return;
			}
		}
	}


	virtual void DeleteAll()
	{
		for (uint32_t i = 0; i < m_Props.size(); i++)
		{
			IProperty *pprop = m_Props[i];
			pprop->Release();
		}

		m_Props.clear();
		m_mapProps.clear();
	}


	virtual size_t GetPropertyCount()
	{
		return m_Props.size();
	}


	virtual IProperty *GetProperty(size_t idx)
	{
		if (idx < m_Props.size())
			return m_Props[idx];

		return NULL;
	}


	virtual IProperty *GetPropertyById(FOURCHARCODE propid)
	{
		TPropertyMap::iterator j = m_mapProps.find(propid);
		if (j != m_mapProps.end())
			return j->second;

		return NULL;
	}


	virtual IProperty *GetPropertyByName(const TCHAR *propname)
	{
		TPropertyArray::const_iterator e = m_Props.end();
		for (TPropertyArray::iterator i = m_Props.begin(); i != e; i++)
		{
			IProperty *pprop = *i;

			if (!_tcsicmp(pprop->GetName(), propname))
				return pprop;
		}

		return NULL;
	}


	virtual CPropertySet &operator =(IPropertySet *propset)
	{
		DeleteAll();

		for (uint32_t i = 0; i < propset->GetPropertyCount(); i++)
		{
			IProperty *pother = propset->GetProperty(i);
			if (!pother)
				continue;

			IProperty *pnew = CreateProperty(pother->GetName(), pother->GetID());
			if (pnew)
				pnew->SetFromProperty(pother);
		}

		return *this;
	}


	virtual CPropertySet &operator +=(IPropertySet *propset)
	{
		AppendPropertySet(propset);

		return *this;
	}


	virtual void AppendPropertySet(IPropertySet *propset)
	{
		for (uint32_t i = 0; i < propset->GetPropertyCount(); i++)
		{
			IProperty *po = propset->GetProperty(i);
			if (!po)
				continue;

			IProperty *pp = this->GetPropertyById(po->GetID());
			if (!pp)
			{
				pp = CreateProperty(po->GetName(), po->GetID());

				if (pp)
					pp->SetFromProperty(po);
			}
		}
	}

	virtual bool Serialize(IProperty::SERIALIZE_MODE mode, BYTE *buf, size_t bufsize, size_t *amountused) const
	{
		size_t used = sizeof(short);

		for (TPropertyMap::const_iterator it = m_mapProps.begin(), last_it = m_mapProps.end(); it != last_it; it++)
		{
			size_t pused = 0;
			CProperty *p = (CProperty *)(it->second);
			p->Serialize(mode, nullptr, 0, &pused);
			used += pused;
		}

		if (amountused)
			*amountused = used;

		if (used > bufsize)
			return false;

		*((short *)buf) = short(m_mapProps.size());
		bufsize -= sizeof(short);

		for (TPropertyMap::const_iterator it = m_mapProps.begin(), last_it = m_mapProps.end(); it != last_it; it++)
		{
			size_t pused = 0;
			CProperty *p = (CProperty *)(it->second);
			if (!p->Serialize(mode, buf, bufsize, &pused))
				return false;

			buf += pused;
			bufsize -= pused;
		}

		return true;
	}

	virtual bool Deserialize(BYTE *buf, size_t bufsize)
	{
		if (!buf)
			return false;

		short numprops = *((short *)buf);
		buf += sizeof(short);
		bufsize -= sizeof(short);

		for (short i = 0; i < numprops; i++)
		{
			BYTE *tmp = buf;
			tmp += sizeof(BYTE);	// serialize mode
			FOURCHARCODE id = *((FOURCHARCODE *)tmp);

			size_t bytesconsumed;

			CProperty *p = (CProperty *)GetPropertyById(id);
			if (!p)
			{
				CProperty *p = new CProperty();
				p->SetID(id);
				AddProperty(p);
			}

			if (!p || !p->Deserialize(buf, bufsize, &bytesconsumed))
				return false;

			buf += bytesconsumed;
			bufsize -= bytesconsumed;
		}

		return true;
	}
};

IPropertySet *IPropertySet::CreatePropertySet()
{
	return new CPropertySet();
}
